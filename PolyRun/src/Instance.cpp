#include "Instance.h"
#include "Random.h"
#include <thread>

#define BUFSIZE 2048

Instance::Instance(std::wstring folder) : m_cmd_line(L"XScrape.exe")
{
	m_inst_status.img_count = 0;
	memset(m_inst_status.status, 0x0, 50);
	strcpy_s(m_inst_status.status,"Uninitialized");
	memset(m_inst_status.domain, 0x0, 50);

	m_saatrib.bInheritHandle = TRUE;
	m_saatrib.lpSecurityDescriptor = NULL;
	m_saatrib.nLength = sizeof(SECURITY_ATTRIBUTES);

	ZeroMemory(&m_procinf, sizeof(PROCESS_INFORMATION));

	std::string rand_name(Random::String(6));

	m_pipename = L"\\\\.\\pipe\\";
	m_pipename.append(std::wstring(rand_name.begin(), rand_name.end()));

	folder.push_back('"');
	folder.insert(folder.begin(), '"');

	m_cmd_line.push_back(L' ');
	m_cmd_line.append(folder);
	m_cmd_line.push_back(L' ');
	m_cmd_line.append(m_pipename);

	mh_INFO_rd = CreateNamedPipeW(
		m_pipename.begin()._Unwrapped(), 
		PIPE_ACCESS_INBOUND,        
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
		1,                      
		0,                        
		0,                        
		NMPWAIT_USE_DEFAULT_WAIT, 
		NULL                       
	);

	if (mh_INFO_rd == INVALID_HANDLE_VALUE)
		m_error = true;

	//STDOUT
	if (!CreatePipe(&mh_OUT_rd, &mh_OUT_wr, &m_saatrib, 0))
		m_error = true;

	//Ensure we do not pass the read end of the pipe
	if (!SetHandleInformation(mh_OUT_rd, HANDLE_FLAG_INHERIT, 0))
		m_error = true;

	//STDIN
	if (!CreatePipe(&mh_IN_rd, &mh_IN_wr, &m_saatrib, 0))
		m_error = true;

	//Ensure we do not pass the write end of the pipe
	if (!SetHandleInformation(mh_IN_wr, HANDLE_FLAG_INHERIT, 0))
		m_error = true;

	if (m_error)
		return;
}

Instance::~Instance()
{
	DWORD exit_code = STILL_ACTIVE;
	GetExitCodeProcess(m_procinf.hProcess, &exit_code);
	if (exit_code == STILL_ACTIVE)
		TerminateProcess(m_procinf.hProcess, 0);

	if (mh_IN_wr != INVALID_HANDLE_VALUE)
		CloseHandle(mh_IN_wr);
	if (mh_OUT_rd != INVALID_HANDLE_VALUE)
		CloseHandle(mh_OUT_rd);
	if (m_procinf.hProcess != INVALID_HANDLE_VALUE)
		CloseHandle(m_procinf.hProcess);
	if (m_procinf.hThread != INVALID_HANDLE_VALUE)
		CloseHandle(m_procinf.hThread);
}

void Instance::Run()
{
	if (m_error)
		return;

	CreateChildProc();

	if (m_error)
		return;

	if (!ConnectNamedPipe(mh_INFO_rd, NULL))
	{
		m_error = true;
		return;
	}

	std::thread t(&Instance::CommLoop, this);
	t.detach();
	std::thread t2(&Instance::CommLoopInfo, this);
	t2.detach();
}

bool Instance::Error() const
{
	return m_error;
}

const std::string& Instance::GetOutput() const
{
	return m_inst_output;
}

const Instance::status& Instance::GetStatus() const
{
	return m_inst_status;
}

DWORD Instance::SendInput(char* str, size_t count)
{
	DWORD written = NULL;
	BOOL bSuccess = WriteFile(mh_IN_wr, str, count, &written, NULL);
	if (!bSuccess) 
		return 0;
	m_inputreq = false;
	return written;
}

DWORD Instance::SendInput(std::string input)
{
	input.append("\n");
	DWORD written = NULL;
	BOOL bSuccess = WriteFile(mh_IN_wr, input.c_str(), input.length(), &written, NULL);
	if (!bSuccess)
		return 0;
	m_inputreq = false;
	return written;
}

bool Instance::InputRequested() const
{
	return m_inputreq;
}

void CarrigeRemove(std::string& input)
{
	long long nline_pos = -1;

	size_t curr_pos = 0;

	if (input[0] == '\r')
	{
		curr_pos++;
	}

	while (curr_pos < input.size())
	{
		if (input[curr_pos] == '\r' && input[curr_pos+1] != '\n')
		{
			if (curr_pos == 0)
			{
				curr_pos++;
				continue;
			}

			if (nline_pos == -1)
				input.erase(nline_pos + 1, curr_pos - nline_pos - 1);
			else
				input.erase(nline_pos + 1, curr_pos - nline_pos);
			curr_pos = nline_pos + 1;
			continue;
		}
		else if (input[curr_pos] == '\n' || (input[curr_pos] == '\r' && input[curr_pos + 1] == '\n'))
		{
			if (input[curr_pos + 1] == '\n')
				nline_pos = curr_pos + 1;
			else
				nline_pos = curr_pos;
		}
		curr_pos++;
	}
}

void Instance::CommLoop()
{
	DWORD exit_code = STILL_ACTIVE;
	while (exit_code == STILL_ACTIVE)
	{
		GetExitCodeProcess(m_procinf.hProcess, &exit_code);

		DWORD dwRead = NULL;
		CHAR chBuf[BUFSIZE] = { 0 };

		BOOL bSuccess = ReadFile(mh_OUT_rd, chBuf, BUFSIZE, &dwRead, NULL);
		if (!bSuccess)
			break;

		std::string incoming(chBuf, dwRead);

		CarrigeRemove(incoming);

		m_inst_output.insert(m_inst_output.end(),incoming.begin(),incoming.end());

		Sleep(1);
	}

	memset(m_inst_status.status, 0x0, 50);
	strcpy_s(m_inst_status.status, "Exited");
}

void Instance::CommLoopInfo()
{
	DWORD exit_code = STILL_ACTIVE;
	while (exit_code == STILL_ACTIVE)
	{
		GetExitCodeProcess(m_procinf.hProcess, &exit_code);

		DWORD dwRead = NULL;
		CHAR chBuf[BUFSIZE] = { 0 };

		if (!ReadFile(mh_INFO_rd, chBuf, BUFSIZE, &dwRead, NULL))
			break;

		if (dwRead == 5 && strncmp("Input", chBuf, 5) == 0)
			m_inputreq = true;
		else if (dwRead == sizeof(status))
			memcpy(&m_inst_status, chBuf, sizeof(status));

		Sleep(1);
	}
}

void Instance::CreateChildProc()
{
	STARTUPINFO siStartInfo;
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.hStdError = mh_OUT_wr;
	siStartInfo.hStdOutput = mh_OUT_wr;
	siStartInfo.hStdInput = mh_IN_rd;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
	siStartInfo.wShowWindow = FALSE;

	BOOL bSuccess = CreateProcessW(
		NULL,
		const_cast<wchar_t*>(m_cmd_line.c_str()),     // command line 
		NULL,          // process security attributes 
		NULL,          // primary thread security attributes 
		TRUE,          // handles are inherited 
		0,             // creation flags 
		NULL,          // use parent's environment 
		NULL,          // use parent's current directory 
		&siStartInfo,  // STARTUPINFO pointer 
		&m_procinf		// receives PROCESS_INFORMATION
	);  

	if (!bSuccess)
		m_error = true;
	else
	{
		CloseHandle(mh_OUT_wr);
		CloseHandle(mh_IN_rd);
		memset(m_inst_status.status, 0x0, 50);
		strcpy_s(m_inst_status.status,"Initalized");
	}
}
