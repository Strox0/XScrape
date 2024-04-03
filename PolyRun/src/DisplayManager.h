#pragma once
#include <Windows.h>
#include "Instance.h"
#include <memory>

class DisplayManager
{
public:
	DisplayManager();
	void Run();

private:

	void CheckForInput();

	void DisplayCurrScreen();
	void MainScreen(bool new_run = false);
	void InstList(bool new_run = false);
	void NewInstance(bool new_run = false);
	void ViewInstance(size_t it_num);

	enum screen
	{
		none,
		main,
		instlist,
		newinst,
		viewinst
	};

	struct alert
	{
		alert() = default;
		bool is_alert = false;
		ULONG64 prev_alert_t = 0;
	};

	screen m_curr_screen = main;
	bool m_should_exit = false;
	HANDLE mh_stdin = INVALID_HANDLE_VALUE;

	std::vector<std::pair<std::shared_ptr<Instance>, alert>> m_instances;
};

