This is based on the repo used for the book "[Crafting Interpreters][]" by
Robert Nystrom. It has my version of the C code version of the interpreter
through chapter 24 plus several enhancements to the code.

My version is for Visual Studio and includes enhanced REPL input logic that 
allows pasting multiple lines of "Lox" code to feed into the compiler.

[crafting interpreters]: http://craftinginterpreters.com

If you find an error or have a suggestion, please do file an issue here. Thank
you!

## Contributing

If you'd like to do that, great! You can just file bugs here on the repo, or
send a pull request if you're so inclined.

See also my IITRAN project, which leverages the Lox interpreter framework.

## Ports and implementations

Another way to get involved is by sharing your own implementation of Lox. Ports
to other languages are particularly useful since not every reader likes Java and
C. Feel free to add your Lox port or implementation to the wiki:

* [Lox implementations][]

[lox implementations]: https://github.com/munificent/craftinginterpreters/wiki/Lox-implementations


### Prerequisites

Visual Studio - I am currently using Visual Studio insiders.
Note: you need to include setting /TC in the VS C++ project settings to compile as C

## Testing

Robert Nystrom's repo has a full Lox test suite he used to ensure the interpreters 
do what they're supposed to do. The test cases live in `test/` in his repo.

I have my test cases samples in my `test/` folder. 
