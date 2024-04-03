#pragma once
#include <string>
#include <Windows.h>
#include <vector>

class Instance
{
public:

	struct status
	{
		DWORD img_count;
		char domain[50];
		char status[50];
	};

	Instance(std::wstring folder);
	~Instance();

	void Run();

	bool Error() const;

	const std::string& GetOutput() const;
	const status& GetStatus() const;

	DWORD SendInput(char* str, size_t count);
	DWORD SendInput(std::string input);

	bool InputRequested() const;

private:

	void CommLoop();
	void CommLoopInfo();
	void CreateChildProc();

	std::wstring m_cmd_line;
	std::wstring m_pipename;

	std::string m_inst_output;
	
	status m_inst_status;

	SECURITY_ATTRIBUTES m_saatrib;
	PROCESS_INFORMATION m_procinf;

	HANDLE mh_OUT_rd = NULL;
	HANDLE mh_OUT_wr = NULL;
	HANDLE mh_IN_rd = NULL;
	HANDLE mh_IN_wr = NULL;
	HANDLE mh_INFO_rd = NULL;

	bool m_inputreq = false;

	bool m_error = false;
	bool m_should_exit = false;
};