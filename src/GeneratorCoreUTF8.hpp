#pragma once
#define UTG_USE_UTF8
#ifdef UTG_USE_UTF8
#include "GeneratorCore.hpp"
#include "utf8.h"

namespace UTG
{
	class InputUTF8
	{
	public:
		UTG::Input::Error init(const utgstr& str, UTG::Input::InputType type) noexcept;
		utgstr& process() noexcept;

		UTG::Input::Result operator[](const utgstr& str) noexcept;
		UTG::Input::Result at(const utgstr& str) noexcept;

		utgstr& get() noexcept;
		utgstr& getOriginal() noexcept;
	private:
		void preprocess() noexcept;

		utgstr original;
		utgstr mod;
		std::vector<std::pair<utgstr, utgstr>> arr;
	};
}
#endif