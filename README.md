# UntitledTextGenerator
An library that takes a text template and allows you to maniputate the variables to be substituted
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
    auto return = input.init("The quick brown ${{ fox }} jumps over the lazy ${{ dog }}!", INPUT_TYPE_MEMORY)
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
