#include "CfgParser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

CfgParser::CfgParser() : m_error(true,"Couldn't locate config file")
{
	std::vector<std::filesystem::path> paths;
	for (auto& i : std::filesystem::directory_iterator(std::filesystem::current_path()))
	{
		if (!i.is_regular_file())
			continue;

		if (i.path().extension() == ".xapi")
			paths.push_back(i.path());
			
	}

	if (paths.empty())
		return;

	if (paths.size() == 1)
	{
		m_filep = paths[0];
		m_error.Reset();
		Parse();
	}
	else
	{
		std::cout << "Multiple config files found (" << paths.size() << ") :";

		for (size_t i = 0; i < paths.size(); i++)
		{
			std::cout << "\t\n[" << i << "] " << paths[i];
		}

		std::cout << "\nChoose : ";
		size_t id = 0;
		std::cin >> id;

		if (id >= paths.size())
		{
			m_error.Set("Incorrect config option");
			return;
		}

		m_filep = paths[id];
		m_error.Reset();
		Parse();
	}
	std::cout << "Opened : " << m_filep << '\n';
}

CfgParser::CfgParser(std::filesystem::path path) : m_error(true, "Couldn't locate config file")
{
	if (std::filesystem::is_regular_file(path) && path.extension() == ".xapi")
	{
		m_filep = path;
		m_error.Reset();
		Parse();
		return;
	}
	else if (std::filesystem::is_directory(path))
	{
		std::vector<std::filesystem::path> paths;
		for (auto& i : std::filesystem::directory_iterator(path))
		{
			if (!i.is_regular_file())
				continue;

			if (i.path().extension() == ".xapi")
				paths.push_back(i.path());

		}

		if (paths.empty())
			return;

		if (paths.size() == 1)
		{
			m_filep = paths[0];
			m_error.Reset();
			Parse();
		}
		else
		{
			std::cout << "Multiple config files found (" << paths.size() << ") :";

			for (size_t i = 0; i < paths.size(); i++)
			{
				std::cout << "\t\n[" << i << "] " << paths[i];
			}

			std::cout << "\nChoose : ";
			size_t id = 0;
			std::cin >> id;

			if (id >= paths.size())
			{
				m_error.Set("Incorrect config option");
				return;
			}

			m_filep = paths[id];
			m_error.Reset();
			Parse();
		}
	}
	std::cout << "Opened : " << m_filep << '\n';
}

_Error CfgParser::Error()
{
	return m_error;
}

_Data CfgParser::Data()
{
	return m_data;
}

static inline bool OnlyNums(std::string_view str) 
{
	for (size_t i = 0; i < str.length(); i++)
	{
		if (!(str[i] >= '0' && str[i] <= '9'))
			return false;
	}
	return true;
}

void Sanatize(std::string& input)
{
	bool space_protected = false;
	bool slash_space_protected = false;
	for (size_t i = 0; i < input.length(); i++)
	{
		if (i < input.size() - 1 && input[i] == '\\' && input[i + 1] == ';')
		{
			space_protected = !space_protected;
			slash_space_protected = !slash_space_protected;
			++i;
			continue;
		}

		if (i < input.size() - 1 && input[i] == '\"' && !slash_space_protected)
		{
			space_protected = !space_protected;
			continue;
		}

		if (std::isspace(input[i]) && !space_protected)
		{
			input.erase(i, 1);
			i--;
		}
	}
}

void CfgParser::Parse()
{
	if (m_error)
		return;

	std::ifstream file(m_filep);

	if (!file.is_open())
	{
		m_error.Set("Couldn't open file");
		return;
	}

	bool domain_found = false;
	bool target_found = false;
	bool limit_found = false;

	std::string line;
	int line_count = 0;
	while (std::getline(file, line, '\n'))
	{
		line_count++;
		Sanatize(line);

		if (line.empty())
			continue;

		size_t equ = line.find('=');
		if (equ == std::string::npos)
		{
			m_error.Set("Incorrect format : expected char '=' (Line " + std::to_string(line_count) + ")");
			return;
		}
		std::string type = line.substr(0, equ);

		if (type == "domain")
		{
			if (domain_found)
			{
				m_error.Set("Incorrect data : multiple domains defined (Line " + std::to_string(line_count) + ")");
				return;
			}

			size_t quo_s = line.find('"');
			if (quo_s == std::string::npos)
			{
				m_error.Set("Incorrect format : expected char '\"' (Line " + std::to_string(line_count) + ")");
				return;
			}
			line.erase(0, quo_s+1);
			size_t quo_e = line.find('"');
			if (quo_e == std::string::npos)
			{
				m_error.Set("Incorrect format : expected char '\"' (Line " + std::to_string(line_count) + ")");
				return;
			}

			m_data.domain = line.substr(0, quo_e);
			domain_found = true;
		}
		else if (type == "target")
		{
			if (target_found)
			{
				m_error.Set("Incorrect data : multiple targets defined");
				return;
			}

			size_t quo_s = line.find('"');
			if (quo_s == std::string::npos)
			{
				m_error.Set("Incorrect format : expected char '\"' (Line " + std::to_string(line_count) + ")");
				return;
			}
			line.erase(0, quo_s+1);
			size_t quo_e = line.find('"');
			if (quo_e == std::string::npos)
			{
				m_error.Set("Incorrect format : expected char '\"' (Line " + std::to_string(line_count) + ")");
				return;
			}

			m_data.target = line.substr(0, quo_e);
			target_found = true;
		}
		else if (type == "incr")
		{
			size_t quo_s = line.find('[');
			if (quo_s == std::string::npos)
			{
				m_error.Set("Incorrect format : expected char '[' (Line " + std::to_string(line_count) + ")");
				return;
			}

			size_t quo_e = line.find(']');
			if (quo_e == std::string::npos)
			{
				m_error.Set("Incorrect format : expected char ']' (Line " + std::to_string(line_count) + ")");
				return;
			}

			std::stringstream dataS(line.substr(quo_s + 1, quo_e - quo_s - 1));

			int var_count = 0;
			std::string token;
			_Data::Incr incr;
			while (std::getline(dataS, token, ','))
			{
				var_count++;
				switch (var_count)
				{
				case 1:
				{
					if (token.starts_with('"'))
					{
						size_t quo_e = token.find_last_of('"');
						token = token.substr(1, quo_e - 1);

						if (!target_found)
						{
							m_error.Set("Incorrect format : 'target' not defined before search based increment position acquiry (Line " + std::to_string(line_count) + ")");
							return;
						}

						size_t pos = m_data.target.find(token) + token.length();
						if (pos == std::string::npos)
						{
							m_error.Set("Incorrect value : search string for increment position not found in 'target' (Line " + std::to_string(line_count) + ")");
							return;
						}
						incr.pos = pos;
						break;
					}

					if (token == "-")
					{
						incr.pos = -1;
						break;
					}

					if (!OnlyNums(token))
					{
						m_error.Set("Incorrect format : non numerical charachters found where not expected (Line " + std::to_string(line_count) + ")");
						return;
					}

					incr.pos = std::stoull(token);
					break;
				}
				case 2:
					if (!OnlyNums(token))
					{
						m_error.Set("Incorrect format : non numerical charachters found where not expected (Line " + std::to_string(line_count) + ")");
						return;
					}
					incr.length = std::stoull(token);
					if (incr.length < 1)
					{
						m_error.Set("Incorrect value : increment length can not be less than 1 (Line " + std::to_string(line_count) + ")");
						return;
					}
					break;
				case 3:
					if (!OnlyNums(token))
					{
						m_error.Set("Incorrect format : non numerical charachters found where not expected (Line " + std::to_string(line_count) + ")");
						return;
					}
					incr.ammount = std::stoull(token);
					if (incr.ammount < 1)
					{
						m_error.Set("Incorrect value : increment ammount can not be less than 1 (Line " + std::to_string(line_count) + ")");
						return;
					}
					break;
				default:
					m_error.Set("Incorrect format : incr doesn't take " + std::to_string(var_count) + " arguments " + "(Line " + std::to_string(line_count) + ")");
					return;
				}
			}
			m_data.increments.push_back(incr);
		}
		else if (type == "bounds")
		{
			std::vector<_Data::Bounds> bounds;

			while (true)
			{
				size_t bra_s = line.find('{');
				if (bra_s == std::string::npos)
				{
					m_error.Set("Incorrect format : expected char '{' (Line " + std::to_string(line_count) + ")");
					return;
				}
				line.erase(0, bra_s + 1);
				size_t bra_e = line.rfind('}');
				if (bra_e == std::string::npos)
				{
					m_error.Set("Incorrect format : expected char '}' (Line " + std::to_string(line_count) + ")");
					return;
				}

				std::string data = line.substr(0, bra_e);

				_Data::Bounds bds;

				size_t quo_e = ParseBoundDataMember(line_count, data, bds.expr);
				if (quo_e == std::string::npos)
					return;

				data.erase(0, quo_e + 2);

				quo_e = ParseBoundDataMember(line_count, data, bds.group_num);
				if (quo_e == std::string::npos)
					return;
				if (!OnlyNums(bds.group_num))
				{
					m_error.Set("Incorrect format : non numerical charachters found where not expected (Line " + std::to_string(line_count) + ")");
					return;
				}

				bounds.push_back(bds);

				if (data[quo_e + 2] != '{')
					break;
			}

			m_data.bounds.push_back(bounds);
		}
		else if (type == "limit")
		{
			if (limit_found)
			{
				m_error.Set("Incorrect data : multiple limits defined (Line " + std::to_string(line_count) + ")");
				return;
			}
			std::string num = line.substr(equ + 1);
			if (!OnlyNums(num))
			{
				m_error.Set("Incorrect format : non numerical charachters found where not expected (Line " + std::to_string(line_count) + ")");
				return;
			}
			m_data.limit = std::stoull(num);
			if (m_data.limit < 1)
			{
				m_error.Set("Incorrect value : limit can not be less than 1 (Line " + std::to_string(line_count) + ")");
				return;
			}
			limit_found = true;
		}
		else if (type == "append")
		{
			size_t quo_s1 = line.find('[');
			if (quo_s1 == std::string::npos)
			{
				m_error.Set("Incorrect format : expected char '[' (Line " + std::to_string(line_count) + ")");
				return;
			}
			line.erase(0, quo_s1 + 1);
			size_t quo_e1 = line.find(']');
			if (quo_e1 == std::string::npos)
			{
				m_error.Set("Incorrect format : expected char ']' (Line " + std::to_string(line_count) + ")");
				return;
			}

			std::string data = line.substr(0, quo_e1);
			std::stringstream ss(data);

			int var_count = 0;
			std::string token;
			_Data::Append appd;
			while (std::getline(ss, token, ','))
			{
				var_count++;
				switch (var_count)
				{
				case 1:
				{
					size_t quo_s = token.find('"');
					if (quo_s == std::string::npos)
					{
						m_error.Set("Incorrect format : expected char '\"' (Line " + std::to_string(line_count) + ")");
						return;
					}
					token.erase(0, quo_s + 1);
					size_t quo_e = token.find('"');
					if (quo_e == std::string::npos)
					{
						m_error.Set("Incorrect format : expected char '\"' (Line " + std::to_string(line_count) + ")");
						return;
					}

					appd.pre_str = token.substr(0, quo_e);
					break;
				}
				case 2:
				{
					bool negative = (token[0] == '-');
					if (negative)
						token.erase(0, 1);
					if (!OnlyNums(token))
					{
						m_error.Set("Incorrect format : non numerical charachters found where not expected (Line " + std::to_string(line_count) + ")");
						return;
					}
					appd.append_pos = std::stoi(token);
					if (negative)
						appd.append_pos *= -1;
					break;
				}
				case 3:
				{
					size_t quo_s = token.find('"');
					if (quo_s == std::string::npos)
					{
						m_error.Set("Incorrect format : expected char '\"' (Line " + std::to_string(line_count) + ")");
						return;
					}
					token.erase(0, quo_s + 1);
					size_t quo_e = token.find('"');
					if (quo_e == std::string::npos)
					{
						m_error.Set("Incorrect format : expected char '\"' (Line " + std::to_string(line_count) + ")");
						return;
					}

					appd.user_msg = token.substr(0, quo_e);
					break;
				}
				default:
					m_error.Set("Incorrect format : append doesn't take " + std::to_string(var_count) + " arguments " + "(Line " + std::to_string(line_count) + ")");
					return;
				}
			}
			m_data.appends.push_back(appd);
		}
		else if (type == "cqfname")
		{
			std::string expr = line.substr(equ + 1);

			if (expr == "false")
			{
				m_data.conseq_filenames = false;
			}
			else if (expr == "true")
			{
				m_data.conseq_filenames = true;
			}
			else
			{
				m_error.Set("Incorrect parameter : value can only be true/false (Line " + std::to_string(line_count) + ")");
				return;
			}
		}
		else
		{
			m_error.Set("Incorrect format : unrecognized typename (Line " + std::to_string(line_count) + ")");
			return;
		}
	}
	
	if (m_data.increments.size() != 0)
	{
		int last_pos_count = 0;
		for (auto& i : m_data.increments)
		{
			if (i.pos != -1)
			{
				if (i.pos+i.length-1 >= m_data.target.size())
				{
					m_error.Set("Incorrect value : increment range exceeding target length");
					return;
				}
			
				std::string mod_nums = m_data.target.substr(i.pos, i.length);

				if (!OnlyNums(mod_nums))
				{
					m_error.Set("Incorrect value : non numerical charachters found at increment position");
					return;
				}
			}
			else
			{
				last_pos_count++;
				if (last_pos_count > 1)
				{
					m_error.Set("Incorrect value : multiple increment positions set to the end of 'target'");
					return;
				}
			}
		}
	}

	if (m_data.appends.size() != 0)
	{
		for (auto& i : m_data.appends)
		{
			if (i.append_pos == -1)
				continue;

			if (i.append_pos < -1)
			{
				m_error.Set("Incorrect value : append value can not be less than -1");
				return;
			}

			if (i.append_pos >= m_data.target.size())
			{
				m_error.Set("Incorrect value : append position outside of target");
				return;
			}
		}
	}
}

size_t CfgParser::ParseBoundDataMember(int line_count, std::string_view data, std::string& outData)
{
	size_t quo_s = data.find("\\;");
	if (quo_s == std::string::npos)
	{
		m_error.Set("Incorrect format : expected char '\\;' (Line " + std::to_string(line_count) + ")");
		return std::string::npos;
	}
	size_t quo_e = data.find("\\;", quo_s + 2);
	if (quo_e == std::string::npos)
	{
		m_error.Set("Incorrect format : expected char '\\;' (Line " + std::to_string(line_count) + ")");
		return std::string::npos;
	}
	
	outData = data.substr(quo_s + 2, quo_e - quo_s - 2);

	return quo_e;
}
