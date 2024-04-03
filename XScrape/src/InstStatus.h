#pragma once
#include <string.h>

struct inst_status
{
	unsigned long img_count;
	char domain[50] = { 0 };
	char status[50] = { 0 };

	inst_status(const char* dom,const char* stat, unsigned long img_count) : img_count(img_count)
	{
		size_t len_do = strnlen_s(dom, 50);
		size_t len_st = strnlen_s(stat, 50);

		strncpy_s(domain, dom, len_do);
		strncpy_s(status, stat, len_st);
	}
};