// Simple Calculator -- a single-file C++17 expression calculator.
//
// Copyright 2025-2026 Khanya Malesela
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// ---------------------------------------------------------------------------
//
// Usage:
//   ./calc                 start the interactive REPL
//   ./calc "3 + 4 * 2"     evaluate one expression, print the result, exit
//
// Everything lives in this one translation unit. `tests.cpp` gets at the
// internals by defining CALCULATOR_NO_MAIN before including this file, which
// is why `main` is the only thing wrapped in an #ifdef below.

#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace calc {

constexpr const char* kVersion = "2.0.0";

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------

/// Every problem caused by user input. Messages are written to be shown to a
/// human as-is: capitalised, no trailing newline, no "error:" prefix (callers
/// add the prefix so it stays consistent between the REPL and the CLI).
class CalcError : public std::runtime_error {
public:
    explicit CalcError(std::string message) : std::runtime_error(std::move(message)) {}
};

namespace {

unsigned char as_uchar(char c) { return static_cast<unsigned char>(c); }

std::string quote(const std::string& text) { return "'" + text + "'"; }

std::string to_lower(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(as_uchar(c)));
    }
    return text;
}

std::string trim(const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

/// Rejects the infinities and NaNs that `<cmath>` happily returns, so a bad
/// argument turns into a readable message instead of a silent "inf".
double require_finite(double value, const std::string& what) {
    if (std::isnan(value)) {
        throw CalcError(what + " is undefined for that argument");
    }
    if (std::isinf(value)) {
        throw CalcError(what + " is too large to represent");
    }
    return value;
}

// ---------------------------------------------------------------------------
// Lexer
// ---------------------------------------------------------------------------

enum class TokenKind { Number, Ident, Plus, Minus, Star, Slash, Percent, Caret, LParen, RParen, End };

struct Token {
    TokenKind kind{TokenKind::End};
    std::string text;  // Source text; used verbatim in error messages.
    double value{0.0}; // Meaningful only for Number.
};

TokenKind operator_kind(char c) {
    switch (c) {
        case '+': return TokenKind::Plus;
        case '-': return TokenKind::Minus;
        case '*': return TokenKind::Star;
        case '/': return TokenKind::Slash;
        case '%': return TokenKind::Percent;
        case '^': return TokenKind::Caret;
        case '(': return TokenKind::LParen;
        case ')': return TokenKind::RParen;
        default: return TokenKind::End;  // Caller reports "unexpected character".
    }
}

std::vector<Token> tokenize(const std::string& input) {
    std::vector<Token> tokens;
    std::size_t i = 0;

    while (i < input.size()) {
        const char c = input[i];

        if (std::isspace(as_uchar(c)) != 0) {
            ++i;
            continue;
        }

        // Numbers: digits, one optional dot, optional exponent.
        if (std::isdigit(as_uchar(c)) != 0 || c == '.') {
            const std::size_t start = i;
            bool seen_digit = false;
            bool seen_dot = false;

            while (i < input.size()) {
                const char d = input[i];
                if (std::isdigit(as_uchar(d)) != 0) {
                    seen_digit = true;
                    ++i;
                } else if (d == '.' && !seen_dot) {
                    seen_dot = true;
                    ++i;
                } else {
                    break;
                }
            }

            // A second dot is never part of a number, so report "1.2.3" as
            // one malformed number rather than as two numbers side by side.
            if (i < input.size() && input[i] == '.') {
                std::size_t j = i + 1;
                while (j < input.size() && std::isdigit(as_uchar(input[j])) != 0) {
                    ++j;
                }
                throw CalcError(quote(input.substr(start, j - start)) + " is not a valid number");
            }

            // An exponent only counts if a signed/unsigned digit follows it,
            // so "2e" stays "2" followed by the constant e rather than a
            // half-consumed number.
            if (seen_digit && i < input.size() && (input[i] == 'e' || input[i] == 'E')) {
                std::size_t probe = i + 1;
                if (probe < input.size() && (input[probe] == '+' || input[probe] == '-')) {
                    ++probe;
                }
                if (probe < input.size() && std::isdigit(as_uchar(input[probe])) != 0) {
                    while (probe < input.size() && std::isdigit(as_uchar(input[probe])) != 0) {
                        ++probe;
                    }
                    i = probe;
                }
            }

            const std::string text = input.substr(start, i - start);
            if (!seen_digit) {
                throw CalcError(quote(text) + " is not a valid number");
            }

            char* end = nullptr;
            const double value = std::strtod(text.c_str(), &end);
            if (end != text.c_str() + text.size()) {
                throw CalcError(quote(text) + " is not a valid number");
            }
            if (std::isinf(value)) {
                throw CalcError(quote(text) + " is too large to represent");
            }
            tokens.push_back(Token{TokenKind::Number, text, value});
            continue;
        }

        // Identifiers: function names and constants.
        if (std::isalpha(as_uchar(c)) != 0 || c == '_') {
            const std::size_t start = i;
            while (i < input.size() &&
                   (std::isalnum(as_uchar(input[i])) != 0 || input[i] == '_')) {
                ++i;
            }
            tokens.push_back(Token{TokenKind::Ident, input.substr(start, i - start), 0.0});
            continue;
        }

        const TokenKind kind = operator_kind(c);
        if (kind == TokenKind::End) {
            throw CalcError(std::string("unexpected character ") + quote(std::string(1, c)));
        }
        tokens.push_back(Token{kind, std::string(1, c), 0.0});
        ++i;
    }

    tokens.push_back(Token{TokenKind::End, "", 0.0});
    return tokens;
}

// ---------------------------------------------------------------------------
// Built-in functions and constants
// ---------------------------------------------------------------------------

double fn_sqrt(double x) {
    if (x < 0.0) {
        throw CalcError("sqrt() needs a non-negative argument");
    }
    return std::sqrt(x);
}

double fn_ln(double x) {
    if (x <= 0.0) {
        throw CalcError("ln() needs a positive argument");
    }
    return std::log(x);
}

double fn_log10(double x) {
    if (x <= 0.0) {
        throw CalcError("log() needs a positive argument");
    }
    return std::log10(x);
}

const std::unordered_map<std::string, double (*)(double)>& functions() {
    static const std::unordered_map<std::string, double (*)(double)> table = {
        {"sqrt", fn_sqrt}, {"abs", std::fabs},   {"floor", std::floor},
        {"ceil", std::ceil}, {"round", std::round}, {"exp", std::exp},
        {"ln", fn_ln},     {"log", fn_log10},    {"sin", std::sin},
        {"cos", std::cos}, {"tan", std::tan},
    };
    return table;
}

const std::unordered_map<std::string, double>& constants() {
    static const std::unordered_map<std::string, double> table = {
        {"pi", 3.14159265358979323846},
        {"e", 2.71828182845904523536},
    };
    return table;
}

// ---------------------------------------------------------------------------
// Parser (recursive descent)
//
//   expression := term (('+' | '-') term)*
//   term       := unary (('*' | '/' | '%') unary)*
//   unary      := ('+' | '-') unary | power
//   power      := primary ('^' unary)?        right-associative
//   primary    := number | name | name '(' expression ')' | '(' expression ')'
//
// Unary sits above power on purpose, so "-2 ^ 2" is -(2^2) == -4, matching
// what a mathematician and every calculator agree on.
// ---------------------------------------------------------------------------

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    double parse() {
        if (peek().kind == TokenKind::End) {
            throw CalcError("there is nothing to calculate");
        }
        const double value = parse_expression();
        if (peek().kind != TokenKind::End) {
            throw CalcError("unexpected " + quote(describe(peek())) + " -- did you forget an operator?");
        }
        return value;
    }

private:
    const Token& peek() const { return tokens_[pos_]; }

    const Token& advance() { return tokens_[pos_++]; }

    static std::string describe(const Token& token) {
        if (token.kind == TokenKind::End) {
            return "end of input";
        }
        return token.text;
    }

    double parse_expression() {
        double value = parse_term();
        while (peek().kind == TokenKind::Plus || peek().kind == TokenKind::Minus) {
            const Token op = advance();
            const double rhs = parse_term();
            value = (op.kind == TokenKind::Plus) ? value + rhs : value - rhs;
            value = require_finite(value, "that result");
        }
        return value;
    }

    double parse_term() {
        double value = parse_unary();
        while (peek().kind == TokenKind::Star || peek().kind == TokenKind::Slash ||
               peek().kind == TokenKind::Percent) {
            const Token op = advance();
            const double rhs = parse_unary();
            switch (op.kind) {
                case TokenKind::Star:
                    value = value * rhs;
                    break;
                case TokenKind::Slash:
                    if (rhs == 0.0) {
                        throw CalcError("division by zero is not allowed");
                    }
                    value = value / rhs;
                    break;
                default:  // Percent. fmod keeps 7.5 % 2 == 1.5 instead of
                          // truncating both operands to int the way v1 did.
                    if (rhs == 0.0) {
                        throw CalcError("modulo by zero is not allowed");
                    }
                    value = std::fmod(value, rhs);
                    break;
            }
            value = require_finite(value, "that result");
        }
        return value;
    }

    double parse_unary() {
        if (peek().kind == TokenKind::Plus) {
            advance();
            return parse_unary();
        }
        if (peek().kind == TokenKind::Minus) {
            advance();
            return -parse_unary();
        }
        return parse_power();
    }

    double parse_power() {
        const double base = parse_primary();
        if (peek().kind == TokenKind::Caret) {
            advance();
            const double exponent = parse_unary();  // Right-associative.
            return require_finite(std::pow(base, exponent), "that result");
        }
        return base;
    }

    double parse_primary() {
        const Token& token = peek();

        switch (token.kind) {
            case TokenKind::Number:
                advance();
                return token.value;

            case TokenKind::Ident: {
                const std::string name = to_lower(token.text);
                advance();

                if (peek().kind == TokenKind::LParen) {
                    advance();
                    const std::unordered_map<std::string, double (*)(double)>& fns = functions();
                    const auto fn = fns.find(name);
                    if (fn == fns.end()) {
                        throw CalcError(quote(token.text) + " is not a known function -- try 'help'");
                    }
                    const double arg = parse_expression();
                    if (peek().kind != TokenKind::RParen) {
                        throw CalcError("missing ')' after " + token.text + "(...)");
                    }
                    advance();
                    return require_finite(fn->second(arg), token.text + "()");
                }

                const std::unordered_map<std::string, double>& consts = constants();
                const auto constant = consts.find(name);
                if (constant == consts.end()) {
                    throw CalcError(quote(token.text) + " is not a known name -- try 'help'");
                }
                return constant->second;
            }

            case TokenKind::LParen: {
                advance();
                const double value = parse_expression();
                if (peek().kind != TokenKind::RParen) {
                    throw CalcError("missing ')' -- the parentheses do not balance");
                }
                advance();
                return value;
            }

            default:
                throw CalcError("unexpected " + quote(describe(token)) + " where a value was expected");
        }
    }

    std::vector<Token> tokens_;
    std::size_t pos_{0};
};

}  // namespace

// ---------------------------------------------------------------------------
// Public entry points
// ---------------------------------------------------------------------------

/// Evaluates one expression. Throws CalcError on anything it cannot handle.
double evaluate(const std::string& input) {
    Parser parser(tokenize(input));
    return parser.parse();
}

/// Formats a result the way a person wants to read it: up to 15 significant
/// digits, which is why "0.1 + 0.2" reads as 0.3 rather than
/// 0.30000000000000004.
std::string format_number(double value) {
    if (std::isnan(value)) {
        return "undefined";
    }
    if (std::isinf(value)) {
        return value > 0.0 ? "infinity" : "-infinity";
    }
    std::ostringstream out;
    out << std::setprecision(15) << value;
    return out.str();
}

void print_help(std::ostream& out) {
    out << "Type an expression and press Enter. For example:\n"
           "  3 + 4 * 2          ->  11        (multiplication binds tighter)\n"
           "  (3 + 4) * 2        ->  14        (parentheses override that)\n"
           "  2 ^ 10             ->  1024      (^ is power)\n"
           "  7.5 % 2            ->  1.5       (% works on fractions too)\n"
           "  sqrt(16) + pi      ->  7.14159...\n"
           "\n"
           "Operators:  +  -  *  /  %  ^  and parentheses\n"
           "Functions:  sqrt abs floor ceil round exp ln log sin cos tan\n"
           "Constants:  pi  e          (names are not case-sensitive)\n"
           "Commands:   help, quit (also exit or q)\n";
}

bool is_quit_command(const std::string& line) {
    const std::string command = to_lower(line);
    return command == "quit" || command == "exit" || command == "q" || command == "bye";
}

/// True for "-h" and "--verbose", false for "-5" and "-.5": a leading minus
/// that starts a number must not be mistaken for a command-line option.
bool looks_like_option(const std::string& arg) {
    if (arg.size() < 2 || arg[0] != '-') {
        return false;
    }
    const char second = arg[1];
    return std::isdigit(as_uchar(second)) == 0 && second != '.';
}

/// Read-evaluate-print loop. Returns normally on "quit" or on end of input;
/// a bad line reports an error and the loop carries on.
void run_repl(std::istream& in, std::ostream& out) {
    out << "\n=== SIMPLE CALCULATOR v" << kVersion << " ===\n";
    out << "Type an expression, or 'help'. Type 'quit' to leave.\n";

    std::string line;
    while (true) {
        out << "> " << std::flush;
        if (!std::getline(in, line)) {
            out << '\n';  // End of input (Ctrl-D or a closed pipe).
            break;
        }

        const std::string text = trim(line);
        if (text.empty()) {
            continue;
        }
        if (is_quit_command(text)) {
            break;
        }
        if (text == "help" || text == "?") {
            print_help(out);
            continue;
        }

        try {
            out << "= " << format_number(evaluate(text)) << '\n';
        } catch (const CalcError& error) {
            out << "error: " << error.what() << '\n';
        } catch (const std::exception& error) {
            out << "error: something went wrong (" << error.what() << ")\n";
        }
    }

    out << "Thanks for using the calculator. Goodbye!\n";
}

/// Command-line entry point, split out from main so the tests can drive it.
int run(int argc, char* const argv[], std::istream& in, std::ostream& out, std::ostream& err) {
    std::vector<std::string> args(argv + 1, argv + argc);

    if (!args.empty()) {
        if (args[0] == "-h" || args[0] == "--help") {
            out << "Simple Calculator v" << kVersion << "\n\n"
                << "Usage:\n"
                << "  calc                  start the interactive REPL\n"
                << "  calc EXPRESSION       evaluate one expression and print the result\n"
                << "  calc --help           show this message\n"
                << "  calc --version        show the version\n\n"
                << "Exit status: 0 on success, 1 on an evaluation error, 2 on bad usage.\n";
            return 0;
        }
        if (args[0] == "-v" || args[0] == "--version") {
            out << "Simple Calculator v" << kVersion << '\n';
            return 0;
        }
        if (looks_like_option(args[0])) {
            err << "calc: unknown option " << quote(args[0]) << " (try --help)\n";
            return 2;
        }
    }

    if (args.empty()) {
        run_repl(in, out);
        return 0;
    }

    // Join the remaining arguments so that both `calc "3 + 4"` and the
    // unquoted `calc 3 + 4` work from a shell.
    std::string expression = args[0];
    for (std::size_t i = 1; i < args.size(); ++i) {
        expression += ' ';
        expression += args[i];
    }

    try {
        out << format_number(evaluate(expression)) << '\n';
        return 0;
    } catch (const CalcError& error) {
        err << "calc: " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        err << "calc: something went wrong (" << error.what() << ")\n";
        return 1;
    }
}

}  // namespace calc

#ifndef CALCULATOR_NO_MAIN
int main(int argc, char** argv) {
    return calc::run(argc, argv, std::cin, std::cout, std::cerr);
}
#endif
