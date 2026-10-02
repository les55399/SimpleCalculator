<h1 align="center">🧮 Simple C++ Calculator</h1>
<p align="center">
  A small, dependency-free console calculator written in C++17. It evaluates typed
  expressions with correct operator precedence, parentheses, functions and constants.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg?logo=c%2B%2B&style=flat-square">
  <img src="https://img.shields.io/badge/build-passing-brightgreen?style=flat-square">
  <img src="https://img.shields.io/badge/tests-107%20passing-brightgreen?style=flat-square">
  <img src="https://img.shields.io/badge/dependencies-none-lightgrey?style=flat-square">
  <img src="https://img.shields.io/badge/license-Apache%202.0-orange?style=flat-square">
</p>

<p align="center">
  <a href="https://github.com/les55399/SimpleCalculator/actions/workflows/ci.yml">
    <img src="https://github.com/les55399/SimpleCalculator/actions/workflows/ci.yml/badge.svg" alt="CI status">
  </a>
</p>

---

## 📸 Preview

```console
$ ./calc
=== SIMPLE CALCULATOR v2.0.0 ===
Type an expression, or 'help'. Type 'quit' to leave.
> 3 + 4 * 2
= 11
> (3 + 4) * 2
= 14
> sqrt(16) + pi
= 7.14159265358979
> 7.5 % 2
= 1.5
> 1 / 0
error: division by zero is not allowed
> quit
Thanks for using the calculator. Goodbye!
```

---

## ✨ Features

- 🧮 Addition, subtraction, multiplication, division and modulo
- 📐 Correct operator precedence — `3 + 4 * 2` is `11`, not `14`
- 🔗 Parentheses, and `^` for powers (`2 ^ 3 ^ 2` is `512`; `^` is right-associative)
- ➖ Unary minus that composes properly: `-2 ^ 2` is `-4`, `2 ^ -2` is `0.25`
- 🔢 Decimal and scientific notation — `1.5e2`, `.5`, `1E-2`
- 📊 `7.5 % 2` is `1.5`, using floating-point remainder instead of truncating to `int`
- 🧰 Functions `sqrt abs floor ceil round exp ln log sin cos tan`, plus constants `pi` and `e`
- 🙅 Clear error messages instead of crashes — see below
- 🔁 A REPL that recovers from a bad line and keeps going
- 🖥️ One-shot mode: `./calc "3 + 4 * 2"` prints `11` and exits, so it works in scripts

---

## 🛠️ Building

Everything lives in a single source file, so there is nothing to configure.

### Prerequisites

- A C++17 compiler (`g++` or `clang++`)
- `make` (optional — the `g++` line below works on its own)

### Build and run

```bash
make            # produces ./calc
./calc          # start the REPL
```

Or without make:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -o calc main.cpp
./calc
```

### Command line

```bash
./calc "3 + 4 * 2"      # 11
./calc 3 + 4 * 2        # 11  (quotes are optional)
./calc --help           # usage
./calc --version        # Simple Calculator v2.0.0
```

Exit status is `0` on success, `1` on an evaluation error, `2` on a bad option —
so failures are detectable from a shell script.

---

## ✅ Tests

The suite has **107 checks** covering arithmetic, precedence, error handling,
number formatting, the REPL and the command-line interface. It has no
dependencies and no test framework — just `main.cpp` plus `tests.cpp`.

```bash
make test
```

```
---------------------------------------------
All 107 checks passed.
```

`tests.cpp` reaches the calculator's internals by defining `CALCULATOR_NO_MAIN`
before `#include`-ing `main.cpp`, which keeps the project genuinely single-file.

---

## 🚨 Error handling

Every one of these prints a message and, in the REPL, leaves the session usable:

| Input | Message |
| --- | --- |
| `1 / 0` | `division by zero is not allowed` |
| `5 % 0` | `modulo by zero is not allowed` |
| `sqrt(-1)` | `sqrt() needs a non-negative argument` |
| `ln(0)` | `ln() needs a positive argument` |
| `2 & 3` | `unexpected character '&'` |
| `(1 + 2` | `missing ')' -- the parentheses do not balance` |
| `3 4` | `unexpected '4' -- did you forget an operator?` |
| `1.2.3` | `'1.2.3' is not a valid number` |
| `foo(2)` | `'foo' is not a known function -- try 'help'` |
| `1e308 * 10` | `that result is too large to represent` |
| *(empty line)* | `there is nothing to calculate` |

---

## 📖 Grammar

```
expression := term (('+' | '-') term)*
term       := unary (('*' | '/' | '%') unary)*
unary      := ('+' | '-') unary | power
power      := primary ('^' unary)?        right-associative
primary    := number | name | name '(' expression ')' | '(' expression ')'
```

Unary sits above `power` on purpose, so `-2 ^ 2` is `-(2^2)` — matching ordinary
mathematical convention.

---

## 🗂️ Project layout

```
.
├── main.cpp        the whole calculator (lexer, parser, REPL, CLI)
├── tests.cpp       107 checks, no external test framework
├── Makefile        build, run, test, clean
├── README.md
├── LICENSE         Apache 2.0
├── .editorconfig   consistent indentation across editors
└── .github/workflows/ci.yml
```

---

## 🔧 What changed from version 1

Version 1 asked for a number, an operator and a number in three separate prompts.
Alongside the new expression parser, these defects were fixed:

- **It could loop forever.** Once `cin >>` hit non-numeric input it set `failbit`,
  every later read silently failed, and `choice` kept its last value. Answering
  the "calculate again?" prompt with `yes` left it spinning: fed that input, v1
  produced **8,427,588 lines** in five seconds before being killed, while v2
  prints 9 lines and exits cleanly.
- **`%` silently truncated.** It cast both operands to `int`, so `7.9 % 3`
  reported `1` with no warning, and values outside `int` range were undefined
  behaviour. It now uses `std::fmod`.
- **Results were rounded to 6 significant digits** by the default stream
  precision. Output now uses 15, so `1 / 3` reads `0.333333333333333`.
- **`using namespace std;`** at global scope is gone.
- **Reading `choice` as a single `char`** meant typing `yes` consumed only the
  `y` and left `es` in the buffer to corrupt the next read. Input is now read
  line by line with `std::getline`.
- **Uninitialised variables** were fed to `switch` and to the loop condition
  whenever a read failed.

---

## 📄 License

Released under the [Apache License 2.0](LICENSE).

## 🙋 Author

**Khanya Malesela** — [@les55399](https://github.com/les55399)
