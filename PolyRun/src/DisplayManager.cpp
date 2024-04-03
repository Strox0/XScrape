#include "DisplayManager.h"
#include <Windows.h>
#include <iostream>
#include <ShObjIdl_core.h>
#include <thread>

#define ALERT_DELAY 15000

std::wstring GetFolder(bool hasto_exist)
{
	IFileDialog* fd;
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

	if (FAILED(hr))
	{
		CoUninitialize();
		return std::wstring();
	}

	static bool sec_init = false;
	if (!sec_init)
	{
		hr = CoInitializeSecurity
		(
			NULL,
			-1,
			NULL,
			NULL,
			RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
			RPC_C_IMP_LEVEL_IMPERSONATE,
			NULL,
			0,
			NULL
		);
		sec_init = true;
	}

	if (FAILED(hr))
	{
		CoUninitialize();
		return std::wstring();
	}

	hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fd));

	if (FAILED(hr))
	{
		CoUninitialize();
		return std::wstring();
	}

	FILEOPENDIALOGOPTIONS fos = FOS_PICKFOLDERS;
	if (hasto_exist)
		fos |= FOS_PATHMUSTEXIST;

	hr = fd->SetOptions(fos);

	if (FAILED(hr))
	{
		fd->Release();
		CoUninitialize();
		return std::wstring();
	}

	hr = fd->Show(NULL);

	if (FAILED(hr))
	{
		fd->Release();
		CoUninitialize();
		return std::wstring();
	}

	IShellItem* psi;
	hr = fd->GetResult(&psi);

	if (FAILED(hr))
	{
		fd->Release();
		psi->Release();
		CoUninitialize();
		return std::wstring();
	}

	wchar_t* folder;
	hr = psi->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &folder);

	if (FAILED(hr))
	{
		fd->Release();
		psi->Release();
		CoUninitialize();
		return std::wstring();
	}

	psi->Release();
	fd->Release();
	CoUninitialize();

	return folder;
}

DisplayManager::DisplayManager()
{
	mh_stdin = GetStdHandle(STD_INPUT_HANDLE);
}

void DisplayManager::Run()
{
	std::thread t(&DisplayManager::CheckForInput, this);

	DisplayCurrScreen();

	t.join();
}

void DisplayManager::CheckForInput()
{
	while (!m_should_exit)
	{
		for (auto& [instance,alert] : m_instances)
		{
			if (instance->InputRequested())
			{
				if (GetTickCount64() - alert.prev_alert_t >= ALERT_DELAY)
				{
					alert.prev_alert_t = GetTickCount64();
					alert.is_alert = true;
					MessageBeep(MB_ICONEXCLAMATION);
				}
			}
			else
			{
				if (alert.is_alert)
					alert.is_alert = false;
			}
		}
		Sleep(50);
	}
}

void DisplayManager::DisplayCurrScreen()
{
	screen prev_screen = none;
	while (!m_should_exit)
	{
		bool new_run = false;
		if (prev_screen != m_curr_screen)
		{
			new_run = true;
			prev_screen = m_curr_screen;
		}

		switch (m_curr_screen)
		{
		case DisplayManager::main:
			MainScreen(new_run);
			break;
		case DisplayManager::instlist:
			InstList(new_run);
			break;
		case DisplayManager::newinst:
			NewInstance(new_run);
			break;
		default:
			break;
		}

		Sleep(10);
	}
}

void DisplayManager::MainScreen(bool new_run)
{
	system("cls");

	if (new_run)
	{
		DWORD mode = 0;

		GetConsoleMode(mh_stdin, &mode);
		SetConsoleMode(mh_stdin, mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
	}

	std::cout << "CREATE NEW INSTANCE\t'N'\n";
	std::cout << "VIEW INSTANCE\t\t'V'\n";
	std::cout << "EXIT\t\t\t'ESC'\n";

	INPUT_RECORD ir;
	DWORD count;
	char choice;

	// Wait for a key event
	ReadConsoleInputW(mh_stdin, &ir, 1, &count);

	if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown)
	{
		choice = ir.Event.KeyEvent.uChar.AsciiChar;
		switch (choice)
		{
		case 'N':
		case 'n':
			m_curr_screen = newinst;
			break;
		case 'V':
		case 'v':
			m_curr_screen = instlist;
			break;
		default:
			if (ir.Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE)
			{
				m_should_exit = true;
				m_curr_screen = none;
			}
			break;
		}
	}
}

void DisplayManager::InstList(bool new_run)
{
	if (new_run)
	{
		DWORD mode = 0;

		GetConsoleMode(mh_stdin, &mode);
		SetConsoleMode(mh_stdin, mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));

		system("cls");

		for (int i = 0; i < m_instances.size(); i++)
		{
			const Instance::status& st = m_instances[i].first->GetStatus();

			if (m_instances[i].second.is_alert) //requesting input
				std::cout << "\033[93m";

			std::cout << "Instance [" << i << "] :\n";
			std::cout << "\tDomain : " << st.domain;
			std::cout << "\n\tImage Count : " << st.img_count;
			std::cout << "\n\tStatus : " << st.status << '\n';

			if (m_instances[i].second.is_alert) //requesting input
				std::cout << "\033[0m\n";
		}

		std::cout << "\nRefresh\t\t\t'R'";
		std::cout << "\nView Instance\t\t'V'";
		std::cout << "\nBack to main\t\t'BACKSPACE'";
	}

	INPUT_RECORD ir;
	DWORD count;
	char choice;

	// Wait for a key event
	ReadConsoleInputW(mh_stdin, &ir, 1, &count);

	if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown)
	{
		choice = ir.Event.KeyEvent.uChar.AsciiChar;
		switch (choice)
		{
		case 'V':
		case 'v':
		{
			DWORD mode = 0;

			GetConsoleMode(mh_stdin, &mode);
			SetConsoleMode(mh_stdin, mode | ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);

			size_t inst_num;
			std::cout << "\n\nEnter Instance id to view : ";
			std::cin >> inst_num;

			if (inst_num > m_instances.size())
			{
				system("cls");
				std::cout << "Incorrect ID\n";
				Sleep(500);
				m_curr_screen = main;
				return;
			}

			ViewInstance(inst_num);

			break;
		}
		case 'R':
		case 'r':
		{
			system("cls");

			for (int i = 0; i < m_instances.size(); i++)
			{
				const Instance::status& st = m_instances[i].first->GetStatus();

				if (m_instances[i].second.is_alert) //requesting input
					std::cout << "\033[93m";

				std::cout << "Instance [" << i << "] :\n";
				std::cout << "\tDomain : " << st.domain;
				std::cout << "\n\tImage Count : " << st.img_count;
				std::cout << "\n\tStatus : " << st.status << '\n';

				if (m_instances[i].second.is_alert) //requesting input
					std::cout << "\033[0m\n";
			}

			std::cout << "\nRefresh\t\t\t'R'";
			std::cout << "\nView Instance\t\t'V'";
			std::cout << "\nBack to main\t\t'BACKSPACE'";

			break;
		}
		default:
			if (ir.Event.KeyEvent.wVirtualKeyCode == VK_BACK)
				m_curr_screen = main;
			break;
		}
	}
}

void DisplayManager::NewInstance(bool new_run)
{
	system("cls");
	if (new_run)
	{
		DWORD mode = 0;

		GetConsoleMode(mh_stdin, &mode);
		SetConsoleMode(mh_stdin, mode | (ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));	
	}

	bool use_currdir = true;
	std::wstring folder;

	std::cout << "Use current working directory (Y/N)? ";
	char yn = 0;
	std::cin >> yn;

	if (yn == 'n' || yn == 'N')
	{
		use_currdir = false;
		folder = GetFolder(true);
	}
	else if (yn == 'y' || yn == 'Y')
	{
		folder = L"./";
	}
	else
	{
		std::cout << "Incorrect input\n";
		Sleep(500);
		return;
	}

	m_instances.push_back(std::pair<std::shared_ptr<Instance>, alert>(std::make_shared<Instance>(folder), alert()));
	
	if (m_instances.back().first->Error())
	{
		system("cls");
		std::cout << "Error occured\n";
		m_instances.pop_back();
		Sleep(750);
		m_curr_screen = main;
		return;
	}

	m_instances.back().first->Run(); 

	system("cls");

	if (m_instances.back().first->Error())
	{
		std::cout << "Error occured\n";
		m_instances.pop_back();
	}
	else
		std::cout << "Success\n";

	Sleep(750);
	m_curr_screen = main;
}

void DisplayManager::ViewInstance(size_t it_num)
{
	system("cls");

	std::cout << "Back\t\t'BACKSPACE'\n";

	Instance& inst = *m_instances[it_num].first;

	size_t display_count = 0;
	while (true)
	{
		if (display_count < inst.GetOutput().size())
		{
			std::cout.write(inst.GetOutput().data() + display_count, inst.GetOutput().size() - display_count);
			display_count += inst.GetOutput().size() - display_count;
		}
		else if (display_count > inst.GetOutput().size())
		{
			auto it = std::find(inst.GetOutput().rbegin(), inst.GetOutput().rend(), '\n');

			std::cout << '\r';

			if (it.base() == inst.GetOutput().end())
				display_count = 0;
			else
				display_count = std::distance(inst.GetOutput().begin(), it.base());

			std::cout.write(inst.GetOutput().data() + display_count, inst.GetOutput().size() - display_count);
			display_count += inst.GetOutput().size() - display_count;
		}

		if (inst.InputRequested())
		{
			std::string input;
			std::cin >> input;
			inst.SendInput(input);
		}

		if (GetAsyncKeyState(VK_BACK) & 0x01)
		{
			m_curr_screen = screen::instlist;
			Sleep(100);
			break;
		}
		Sleep(10);
	}
}
