#include "GeneratorCoreUTF8.hpp"
#include <fstream>
#include <string>

UTG::Input::Error UTG::InputUTF8::init(const UTG::utgstr& str, UTG::Input::InputType type) noexcept
{
	if (utf8::find_invalid(str.begin(), str.end()) != str.end())
		return UTG::Input::ERROR_TYPE_INVALID_UTF8;
	if (type == UTG::Input::INPUT_TYPE_FILE)
	{
		std::ifstream in(str);
		if (!in)
			return UTG::Input::ERROR_TYPE_INVALID_FILE;
		utgstr tmp;
		original.clear();
		while (std::getline(in, tmp))
		{
			original += (tmp + '\n');
		}
		mod = original;
		preprocess();
	}
	else if (type == UTG::Input::INPUT_TYPE_MEMORY)
	{
		original = str;
		mod = original;
		preprocess();
		return UTG::Input::ERROR_TYPE_NO_ERROR;
	}
	return UTG::Input::ERROR_TYPE_INVALID_INPUT_TYPE;
}

UTG::utgstr& UTG::InputUTF8::process() noexcept
{
	for (auto& a : arr)
	{
		utgstr tmp = "${{" + a.first + "}}";
		while (true)
		{
			size_t fnd = mod.find(tmp);
			if (fnd != utgstr::npos)
				mod.replace(fnd, tmp.length(), a.second);
			else
				goto escape;
		}
	escape:;
	}
	return mod;
}

UTG::Input::Result UTG::InputUTF8::operator[](const utgstr& str) noexcept
{
	return at(str);
}

UTG::Input::Result UTG::InputUTF8::at(const utgstr& str) noexcept
{
	Input::Result result;
	result.inutf8 = this;
	result.key = str;
	return result;
}

UTG::utgstr& UTG::InputUTF8::get() noexcept
{
	return mod;
}

UTG::utgstr& UTG::InputUTF8::getOriginal() noexcept
{
	return original;
}

void UTG::InputUTF8::preprocess() noexcept
{
	size_t offset = 0;
	while (true)
	{
		size_t fnd = mod.find("${{", offset);
		if (fnd == utgstr::npos)
			break;
		utgstr tmp;
		for (size_t i = (fnd + 3); i < mod.size(); i++)
		{
			if (mod[i] == ' ')
				continue;
			else if (mod[i] == '}' && (i + 1) < mod.size() && mod[i + 1] == '}')
			{
				offset = i - 1;
				goto esc;
			}
			else if ((i + 1) >= mod.size())
			{
				tmp.clear();
				goto esc;
			}
			tmp += mod[i];
		}
	esc:
		if (!tmp.empty())
		{
			bool bFound = false;
			for (auto& a : arr)
			{
				if (a.first == tmp)
				{
					bFound = true;
					goto esc1;
				}
			}
		esc1:
			if (!bFound)
			{
				mod.erase(mod.begin() + fnd + 3, mod.begin() + offset + 1);
				mod.insert(fnd + 3, tmp);

				arr.push_back({ tmp, "" });
			}
		}
	}
}
