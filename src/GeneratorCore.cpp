#include "GeneratorCore.hpp"
#include <fstream>
#include <string>

UTG::Input::Error UTG::Input::init(const utgstr& str, UTG::Input::InputType type) noexcept
{
	if (type == UTG::Input::INPUT_TYPE_FILE)
	{
		std::ifstream in(str);
		if (!in)
			return ERROR_TYPE_INVALID_FILE;
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
		return ERROR_TYPE_NO_ERROR;
	}
	return ERROR_TYPE_INVALID_INPUT_TYPE;
}

UTG::utgstr& UTG::Input::process() noexcept
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

UTG::Input::Result UTG::Input::operator[](const UTG::utgstr& str) noexcept
{
	return at(str);
}

UTG::Input::Result UTG::Input::at(const utgstr& str) noexcept
{
	Input::Result result;
	result.in = this;
	result.key = str;
	return result;
}

UTG::utgstr& UTG::Input::get() noexcept
{
	return mod;
}

UTG::utgstr& UTG::Input::getOriginal() noexcept
{
	return original;
}

void UTG::Input::preprocess() noexcept
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
				
				arr.push_back({tmp, ""});
			}
		}
	}
}

UTG::Input::Result::operator bool() noexcept
{
	for (auto& a : in->arr)
		if (a.first == key)
			return true;
	return false;
}

UTG::Input::Result& UTG::Input::Result::operator=(const utgstr& arg) noexcept
{
	for (auto& a : this->in->arr)
	{
		if (a.first == key)
		{
			a.second = arg;
			break;
		}
	}
	return *this;
}
