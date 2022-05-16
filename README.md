# UntitledTextGenerator
An library that takes a text template and allows you to maniputate the variables to be substituted

Features:
- ANSI string parsing - done
- UTF-8 support - to be added
## Installation
This library can be installed by just copying the contents of this repository into your project and adding them to your build process

The library uses [utfcpp](https://github.com/MadLadSquad/utfcpp) for UTF-8 support, this however can be disabled by defining the macro `UTG_USE_UTF8`. This way you can eliminate the dependency.

If you are using UTF-8 support and you use CMake, you have to add the following line to your CMakeLists.txt file
```cmake
add_subdirectory(dir-to-this-lib/ThirdParty/utfcpp/)
```
On all build systems however, you need to also link with `utf8cpp`
## Usage
Create a file with some text like this
```
The quick brown fox jumps over the lazy dog!
```
With that being set you might want to add a variable for different animals. This can be achieved like this
```
The quick brown ${{ fox }} jumps over the lazy ${{ dog }}!
```
Those variables will then be replaced using the following code
```cpp
#include <iostream>
#include "src/GeneratorCore.hpp"

int main()
{
    UTG::Input input;
    auto return = input.init("The quick brown ${{ fox }} jumps over the lazy ${{ dog }}!", UTG::Input::INPUT_TYPE_MEMORY)
    if (return != ERROR_TYPE_NO_ERROR)
    {
        std::cout << "Error when parsing string! Code: " << static_cast<int>(return) << std::endl;
        return -1;
    }
    input["fox"] = "dog";
    input["dog"] = "fox"
    std::cout << input.process() << std::endl;
}
```
which will produce the following output
```
The quick brown dog jumps over the lazy fox!
```
## Macro definitions
You can define multiple macro definitions to change your desired string implementation:
```cpp
#define UTG_CUSTOM_STRING_TYPE std::string
#define UTG_CUSTOM_STRING_TYPE_INCLUDE <iostream>
```
The `UTG_CUSTOM_STRING_TYPE` macro should define the typename for the type, while the `UTG_CUSTOM_STRING_TYPE_INCLUDE` defines the include path for the given container
## Reference
There are 2 types of input stored under the `InputType` enum under the `UTG::Input` class
```cpp
enum InputType
{
    INPUT_TYPE_FILE, // The input string is a file
	INPUT_TYPE_MEMORY // The input string is the string to be used
};
```

---

Errors are stored as part of the `Error` enum, under the `UTG::Input` class
```cpp
enum Error
{
	ERROR_TYPE_NO_ERROR = 0, // SUCCESS
	ERROR_TYPE_INVALID_FILE = 1, // File location wrong or the file's invalid
	ERROR_TYPE_INVALID_ADDRESS = 2, 
	ERROR_TYPE_INVALID_INPUT_TYPE = 3,
	ERROR_TYPE_INVALID_UTF8 = 4, // Invalid UTF-8 string
	ERROR_TYPE_INVALID_ACCESS = ERROR_TYPE_INVALID_ADDRESS
};
```

---

The `UTG::Input::init` function takes a string and `InputType` as arguments, reads the string/file depending on the `InputType`, preprocesses the text and returns an `Error`

The `UTG::Input::process` function processes the string and does the replacements, then returns the final string, as well as storing it

The `UTG::Input::get` function returns the final string, while the `UTG::Input::getOriginal` function returns the original string passed to the generator

The `UTG::Input::operator[]` and `UTG::Input::at` functions take a string argument and return a result struct which can be used to assign the future replacement strings

---

The `UTG::Input::Result` struct defines an easy way to access the value of the key defined in the string and set it. It defines 2 operators, `operator bool`, for implicit checks and `operator=` for assigning the value at the specific key. This allows the following code to be valid:
```cpp
if (input["fox"])
    input["fox"] = "dog";
```
