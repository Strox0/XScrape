#include <SDKDDKVer.h>
#include <iostream>
#include <chrono>
#include <Windows.h>

#include "CfgParser.h"
#include "Scraper.h"
#include "InstStatus.h"

#undef max

int main(int argc, char* argv[])
{
	if (argc > 3)
	{
		std::cout <<
			"Incorrect number of agruments"
			"\nUsage : "
			"<program name> [cfg.xapi file / directory path]"
			"\nExample :"
			"\n<program name> ../cfg.xapi"
			"\n<program name> \"C:/User/<UserName>/Documents/Configs\"";
		return 1;
	}

	bool managed_instance = (argc == 3);
	HANDLE coms_pipe = INVALID_HANDLE_VALUE;
	CfgParser* parser = nullptr;

	if (argc == 2)
		parser = new CfgParser(argv[1]);
	else if (argc == 3)
	{
		parser = new CfgParser(argv[1]);

		std::string pipe_name = argv[2];

		coms_pipe = CreateFileA(pipe_name.c_str(), GENERIC_WRITE, NULL, NULL, OPEN_EXISTING, 0, NULL);

		if (coms_pipe == INVALID_HANDLE_VALUE)
		{
			MessageBoxA(NULL, "Failed to open pipe", "Error", MB_ICONERROR);
			return 1;
		}

		inst_status stat("\0", "Connected", 0);
		DWORD written = 0;
		WriteFile(coms_pipe, &stat, sizeof(inst_status), &written, NULL);
		
		if (written != sizeof(inst_status))
		{
			MessageBoxA(NULL, "Failed to write to pipe", "Error", MB_ICONERROR);
			return 1;
		}
	}
	else
		parser = new CfgParser();

	_Error err = parser->Error();
	if (err)
	{
		std::cout << "Parser error : \n\t" << err.message << std::endl;
		if (managed_instance)
			CloseHandle(coms_pipe);
		return 1;
	}

	std::string outfolder;

	if (managed_instance)
	{
		std::cout << "Enter output folder name : ";

		const char inputmsg[] = "Input";
		DWORD written = 0;
		WriteFile(coms_pipe, inputmsg, 5, &written, NULL);

		if (written != 5)
		{
			MessageBoxA(NULL, "Failed to write to pipe", "Error", MB_ICONERROR);
			return 1;
		}

		std::cin >> outfolder;
	}
	else
	{
		std::cout << "Enter output folder name (Press enter to use default) : ";
		std::cin.ignore();
		std::cin.clear();
		std::getline(std::cin, outfolder);
	}

	if (outfolder.empty())
		outfolder = "output";

	_Data data = parser->Data();

	std::vector<std::pair<size_t, size_t>> pos_mods;
	for (const auto& i : data.appends)
	{
		std::string input;
		std::cout << i.user_msg;

		if (managed_instance)
		{
			const char inputmsg[] = "Input";
			DWORD written = 0;
			WriteFile(coms_pipe, inputmsg, 5, &written, NULL);

			if (written != 5)
			{
				MessageBoxA(NULL, "Failed to write to pipe", "Error", MB_ICONERROR);
				return 1;
			}
		}

		std::cin >> input;

		input.insert(0, i.pre_str);

		if (i.append_pos != -1)
		{
			size_t pos_mod = 0;
			for (const auto& [pos, modif] : pos_mods)
			{
				if (pos < i.append_pos)
				{
					pos_mod += modif;
				}
			}

			data.target.insert(i.append_pos + pos_mod, input);

			pos_mods.push_back(std::make_pair<size_t, size_t>(i.append_pos, input.size()));
		}
		else
		{
			data.target += input;
		}
	}

	for (auto& i : data.increments)
	{
		size_t pos_mod = 0;
		for (const auto& [pos, modif] : pos_mods)
		{
			if (pos <= i.pos)
			{
				pos_mod += modif;
			}
		}

		if (i.pos == -1)
			i.pos = data.target.size() - i.length;
		else
			i.pos += pos_mod;
	}

	std::cout << "Domain : " << data.domain;
	std::cout << "\nTarget : " << data.target << "\n\n";

	std::chrono::steady_clock::time_point start_timestamp = std::chrono::steady_clock::now();

	if (managed_instance)
	{
		inst_status stat(data.domain.c_str(), "Running", 0);
		DWORD written = 0;

		WriteFile(coms_pipe, &stat, sizeof(inst_status), &written, NULL);

		if (written != sizeof(inst_status))
		{
			MessageBoxA(NULL, "Failed to write to pipe", "Error", MB_ICONERROR);
			return 1;
		}
	}

	int ret_code = 0;
	try
	{
		Scraper scraper(data,outfolder);

		if (managed_instance)
			scraper.PolyRunMode(true,coms_pipe);

		scraper.Start();
	}
	catch (std::exception const& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		ret_code = 1;
	}

	std::cout << "\n\nElapsed time : " << std::chrono::duration<double>(std::chrono::steady_clock::now() - start_timestamp).count() << 's';
	
	if (!managed_instance)
	{
		std::cout << "\nPress ESC to exit\n";

		HWND window = GetConsoleWindow();
		MessageBeep(MB_ICONASTERISK);

		while (true)
		{
			Sleep(1);
			if (GetAsyncKeyState(VK_ESCAPE) & 0x01 && GetForegroundWindow() == window)
				break;
		}
	}
	else
	{
		std::cout << "\nDONE";
		Sleep(5000);
		CloseHandle(coms_pipe);
	}

	return ret_code;
}
