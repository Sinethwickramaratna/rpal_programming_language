# RPAL Compiler

An RPAL compiler and evaluator written in C++ for a Programming Languages project.

The project implements the full pipeline for RPAL programs:

1. lexical analysis
2. parsing
3. AST standardization
4. CSE machine evaluation

## Repository Layout

```text
.
├── README.md
├── RPAL_Grammar.pdf
├── RPAL_Lex.pdf
├── rpal/
│   ├── Makefile
│   ├── rpal.cpp
│   ├── lexical_analyser.cpp
│   ├── parser.cpp
│   ├── standerdize.cpp
│   ├── cse_machine.cpp
│   ├── token.h
│   └── tree.h
└── sample inputs and reference material
```

## Prerequisites

- A C++ toolchain with `gcc`/`g++` support
- `make`

### Ubuntu / Linux

Install the required packages with:

```bash
sudo apt install build-essential
```

### Windows

Use a MinGW/MSYS2 environment with `gcc` and `make` available. If needed,
install MSYS2 and the MinGW toolchain, then run the commands from the MSYS2
MinGW shell.

## Build

### Ubuntu / Linux

Build the project from the `rpal/` folder:

```bash
cd rpal
make
```

### Windows

Build the project from the `rpal/` folder:

```bat
cd rpal
mingw32-make
```

If your environment exposes `make`, you can also use:

```bat
make
```

This produces the executable `rpal20` on Linux and usually `rpal20.exe` on
Windows, depending on the toolchain.

## Run

Run the compiler with an RPAL source file.

### Ubuntu / Linux

```bash
./rpal20 sample.txt
```

### Windows

```bat
./rpal20.exe sample.txt
```

If your toolchain names the executable without `.exe`, use `./rpal20` instead.

Replace `sample.txt` with your own input file.

You can also run any of the provided RPAL test files in the same way, for
example:

```bash
./rpal20 rpal_test_programs/rpal_01
```

or on Windows:

```bat
./rpal20.exe rpal_test_programs\rpal_01
```

### Optional Flags

The program supports two debugging flags:

- `-ast` prints the Abstract Syntax Tree before standardization.
- `-st` prints the Standardized Tree before CSE execution.

Example usage:

```bash
./rpal20 -ast sample.txt
./rpal20 -st sample.txt
```

On Windows:

```bat
./rpal20.exe -ast sample.txt
./rpal20.exe -st sample.txt
```

## How to Run the Code

1. Open a terminal in the project root.
2. Build the project using the commands above for your operating system.
3. Run the executable with the path to an RPAL source file.
4. Read the output printed to the terminal.

If you want to compare against a reference output, redirect the result to a file
and use `diff` or a similar comparison tool.

## Example Output

For an input that evaluates to the integer `15`, the program prints:

```text
15
```

If you use `-ast` or `-st`, the program prints the tree structure instead of
the final evaluated value.

## Output

The program reads a full RPAL source file, then prints the evaluated result.
The output should match the expected RPAL test output for the same program.

The `Print` builtin writes the evaluated value directly, without adding an extra newline.

## Notes

- The main entry point is [rpal/rpal.cpp](rpal/rpal.cpp).
- Parser and standardization logic are split across separate source files for clarity.
- The repository includes the RPAL grammar and lexical specification PDFs for reference.

## Project Status

This project is intended for the CS/Programming Languages RPAL assignment and is
organized as a small teaching repository rather than a general-purpose package.

## Contributing

If you extend the compiler, keep the current file layout and build flow intact.
That makes it easier to compare the implementation against the RPAL grammar and
the expected test outputs.

## License

No explicit license has been added yet. If you plan to publish this repository
publicly, add a license file before sharing it more broadly.
