#pragma once
#include <concepts>
#include <random>
#include <string>

namespace Random
{

	template<typename _T>
	concept Integral = std::is_integral_v<_T>;

	// concept for floating point types
	template<typename _T>
	concept Real = std::is_floating_point_v<_T>;

	template<typename T>
	class Number
	{
	public:
		typedef T type;
		Number(type min, type max) :m_min(min), m_max(max) {};

		void SetRange(type min, type max)
		{
			m_min = min;
			m_max = max;
		}

		type Gen()
		{
			static std::random_device dev;
			static std::mt19937 rng(dev());

			if constexpr (Integral<T>)
			{
				std::uniform_int_distribution<type> dist(m_min, m_max);
				return dist(rng);
			}
			else if constexpr (Real<T>)
			{
				std::uniform_real_distribution<type> dist(m_min, m_max);
				return dist(rng);
			}
		}

	private:
		type m_min;
		type m_max;
	};


	enum CharSetFlags : unsigned int
	{
		Small = (1 << 0),
		Large = (1 << 1),
		Numbers = (1 << 2),
		Special = (1 << 3)
	};

	std::string String(int length, unsigned int set_flags = CharSetFlags::Small)
	{
		std::vector<char> characters;

		if (set_flags & CharSetFlags::Small)
		{
			const char small[] = "abcdefghijklmnopqrstuvwxyz";
			characters.insert(characters.end(), std::begin(small), std::end(small));
			characters.pop_back();
		}
		if (set_flags & CharSetFlags::Large)
		{
			const char large[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
			characters.insert(characters.end(), std::begin(large), std::end(large));
			characters.pop_back();
		}
		if (set_flags & CharSetFlags::Numbers)
		{
			const char num[] = "0123456789";
			characters.insert(characters.end(), std::begin(num), std::end(num));
			characters.pop_back();
		}
		if (set_flags & CharSetFlags::Special)
		{
			const char spec[] = "!@#$%&*";
			characters.insert(characters.end(), std::begin(spec), std::end(spec));
			characters.pop_back();
		}

		std::string str;

		Number<int> r(0, characters.size() - 1);
		for (int i = 0; i < length; i++)
		{
			str += characters[r.Gen()];
		}

		return str;
	};
}