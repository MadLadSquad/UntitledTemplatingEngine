#pragma once
#include <vector>

#define UTG_CUSTOM_STRING_TYPE std::string
#define UTG_CUSTOM_STRING_TYPE_INCLUDE <iostream>
#ifdef UTG_CUSTOM_STRING_TYPE
	#ifndef UTG_CUSTOM_STRING_TYPE_INCLUDE
		#error UTG_CUSTOM_STRING_TYPE_INCLUDE macro not defined
	#else
		#include UTG_CUSTOM_STRING_TYPE_INCLUDE
		namespace UTG
		{
			using utgstr = UTG_CUSTOM_STRING_TYPE;
		}
	#endif
#else
	#include <iostream>	
	namespace UTG
	{
		using utgstr = std::string;
	}
#endif

namespace UTG
{
	class InputUTF8;
	class Input
	{
	public:
		enum InputType
		{
			INPUT_TYPE_FILE,
			INPUT_TYPE_MEMORY
		};
		enum Error
		{
			ERROR_TYPE_NO_ERROR = 0,
			ERROR_TYPE_INVALID_FILE = 1,
			ERROR_TYPE_INVALID_ADDRESS = 2,
			ERROR_TYPE_INVALID_INPUT_TYPE = 3,
			ERROR_TYPE_INVALID_UTF8 = 4,
			ERROR_TYPE_INVALID_ACCESS = ERROR_TYPE_INVALID_ADDRESS
		};

		struct Result
		{
			operator bool() noexcept;
			Result& operator=(const utgstr& arg) noexcept;
			//operator utgstr() noexcept;
			utgstr key;
		private:
			friend class Input;
			friend class InputUTF8;
			Input* in = nullptr;
			InputUTF8* inutf8 = nullptr;
		};

		Error init(const utgstr& str, InputType type) noexcept;
		utgstr& process() noexcept;

		Result operator[](const utgstr& str) noexcept;
		Result at(const utgstr& str) noexcept;

		utgstr& get() noexcept;
		utgstr& getOriginal() noexcept;
	private:
		void preprocess() noexcept;

		utgstr original;
		utgstr mod;
		std::vector<std::pair<utgstr, utgstr>> arr;
	};
}