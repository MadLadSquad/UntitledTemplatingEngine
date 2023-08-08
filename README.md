# UntitledTemplatingEngine
[![MIT license](https://img.shields.io/badge/License-MIT-blue.svg)](https://lbesson.mit-license.org/)
[![trello](https://img.shields.io/badge/Trello-UDE-blue])](https://trello.com/b/HmfuRY2K/untitleddesktop)
[![Discord](https://img.shields.io/discord/717037253292982315.svg?label=&logo=discord&logoColor=ffffff&color=7389D8&labelColor=6A7EC2)](https://discord.gg/4wgH8ZE)

A C and C++ templating engine powered by a lisp-like programming language with plugin support.

## Example template
This template showcases the whole standard library and what the language is capable of:
```liquid
-------------------------------------------------- STRING REPLACEMENT --------------------------------------------------

The {{ at {{ descriptors }} 0 }} {{ colour }} fox {{ at {{ actions }} a1 }} over the {{ at {{ descriptors }} 1 }} dog

------------------------------------------------------ FOR LOOPS -------------------------------------------------------

Arrays: {{ for it {{ descriptors }} {{ func This is {{ it }}
}} }}

Maps: {{ for key val {{ actions }} {{ func Key: {{ key }}
Value: {{ val }}
}} }}

Arrays using the list function: {{ for it {{ list a b c }} {{ func {{ it }} }} }}
Maps using the dict function: {{ for key val {{ dict a b c d e }} {{ func {{ key }}:{{ val }} }} }}

----------------------------------------------------- IF STATEMENTS ----------------------------------------------------

{{ if {{ == {{ value }} test }}
    {{ func {{ test_val }} }}
    {{ func {{ not_test_val }} }}
}}

{{ switch {{ value }}
    test {{ func {{ test_val }} }}
    example {{ func {{ example_val }} }}
    {{ func {{ fallback_val }} }}
}}

{{ cond
    {{ == {{ value }} test }}{{ func {{ test_val }} }}
    {{ == {{ value }} example }}{{ func {{ example_val }} }}
    {{ func {{ fallback_val }} }}
}}

----------------------------------------------------- RAW TEMPLATES ----------------------------------------------------

{{ raw {{ for a arr
    {{ func
        {{ a }}
    }}
}}}}

---------------------------------------------------------- END ---------------------------------------------------------
```
More information about the language can be found on the 
[wiki](https://github.com/MadLadSquad/UntitledTemplatingEngine/wiki/).

## Why?
Because not many templating engines for C and C++ support custom functions. When we first started this project, we 
needed a templating engine for the [UVKBuildTool](https://github.com/MadLadSquad/UVKBuildTool). We first developed a
small library for substituting text, but we found that too much code on the application side was written to just output
text with the correct formatting.

Additionally, we wanted to add a static site generator mode to the 
[UVKBuildTool](https://github.com/MadLadSquad/UVKBuildTool), for which, we needed more advanced templates.

## Size and performance
The parser, including its header file and the whole C++ API is around 340 SLoC. The standard library is around 360 SLoC,
while the C API is around 220 SLoC

Given that this library provides a language interpreter, the parser may be a bit slower than other dumb implementations.
However, given that the parser is really simple and does all the parsing in 1 pass, it's safe to say that it's quite
performat for what it provides.

Additionally, we offer the option to replace `std::string` and `std::map` with other custom implementations, which may
lead to massive performance gains.

## Usage, installation and learning
Documentation can be found on the [wiki](https://github.com/MadLadSquad/UntitledTemplatingEngine/wiki/).

