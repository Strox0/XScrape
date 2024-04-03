#pragma once
#include <filesystem>
#include <string>
#include <vector>

struct _Error
{
	_Error() {};
	_Error(bool error, std::string message) : error(error), message(message) {};
	operator bool() { return error; };
	void Set(std::string msg) { error = true; message = msg; }
	void Reset() { error = false; message.clear(); }
	bool error;
	std::string message;
};

struct _Data 
{
	struct Incr
	{
		Incr() : pos(0), length(0), ammount(0) {};
		Incr(size_t pos, size_t len, size_t ammount) : pos(pos), length(len), ammount(ammount) {};
		size_t pos;
		size_t length;
		size_t ammount;
	};
	struct Bounds
	{
		Bounds() {};
		Bounds(std::string expr, std::string gnum) : expr(expr), group_num(gnum) {};
		std::string expr;
		std::string group_num;
	};
	struct Append
	{
		Append() : append_pos(0) {};
		Append(const std::string& pre_str, int append_pos,const std::string& user_msg) : pre_str(pre_str), append_pos(append_pos),user_msg(user_msg) {}
		std::string pre_str;
		int append_pos;	
		std::string user_msg;
	};
	std::string domain;
	std::string target;
	std::vector<Incr> increments;
	size_t limit = 1;
	std::vector<std::vector<Bounds>> bounds;
	std::vector<Append> appends;
	bool conseq_filenames = false;
};

class CfgParser
{
public:
	CfgParser();
	explicit CfgParser(std::filesystem::path path);

	_Error Error();

	_Data Data();

private:
	void Parse();
	size_t ParseBoundDataMember(int line_count,std::string_view data, std::string& outData);

private:
	std::filesystem::path m_filep;
	_Error m_error;
	_Data m_data;
};