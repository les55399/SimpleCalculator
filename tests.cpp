// Tests for the Simple Calculator.
//
// Copyright 2025-2026 Khanya Malesela
// SPDX-License-Identifier: Apache-2.0
//
// The calculator is a single translation unit, so the tests reach its
// internals the way a single-file project normally does: define
// CALCULATOR_NO_MAIN to suppress main(), then #include the source.
//
// Run them with `make test`.

#define CALCULATOR_NO_MAIN 1
#include "main.cpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_checks = 0;
int g_failures = 0;
std::string g_section;

void section(const std::string& name) {
    g_section = name;
    std::cout << "\n" << name << '\n';
}

void fail(const std::string& detail) {
    ++g_failures;
    std::cout << "  FAIL [" << g_section << "] " << detail << '\n';
}

void pass() {
    ++g_checks;
    std::cout << "  ok   (" << g_checks << " so far)\n";
}

/// The value of an expression, or a marker string when evaluation threw.
struct Outcome {
    bool threw{false};
    double value{0.0};
    std::string message;
};

Outcome eval(const std::string& input) {
    try {
        return Outcome{false, calc::evaluate(input), ""};
    } catch (const calc::CalcError& error) {
        return Outcome{true, 0.0, error.what()};
    }
}

void expect_value(const std::string& input, double expected) {
    const Outcome result = eval(input);
    if (result.threw) {
        fail("'" + input + "' should have evaluated to " + std::to_string(expected) +
             " but threw: " + result.message);
        return;
    }
    if (std::fabs(result.value - expected) > 1e-9) {
        fail("'" + input + "' evaluated to " + calc::format_number(result.value) + ", expected " +
             std::to_string(expected));
        return;
    }
    pass();
}

void expect_error(const std::string& input, const std::string& expected_fragment) {
    const Outcome result = eval(input);
    if (!result.threw) {
        fail("'" + input + "' should have failed but returned " + calc::format_number(result.value));
        return;
    }
    if (result.message.find(expected_fragment) == std::string::npos) {
        fail("'" + input + "' failed with '" + result.message + "', expected it to mention '" +
             expected_fragment + "'");
        return;
    }
    pass();
}

void expect_output(const std::string& what, const std::string& actual, const std::string& fragment) {
    if (actual.find(fragment) == std::string::npos) {
        fail(what + ": output did not contain '" + fragment + "'. Got:\n" + actual);
        return;
    }
    pass();
}

void expect_equal(const std::string& what, const std::string& actual, const std::string& expected) {
    if (actual != expected) {
        fail(what + ": got '" + actual + "', expected '" + expected + "'");
        return;
    }
    pass();
}

void expect_true(const std::string& what, bool condition) {
    if (!condition) {
        fail(what + ": expected true");
        return;
    }
    pass();
}

/// Drives calc::run the same way main would.
int run_cli(const std::string& joined_args, std::string* out, std::string* err) {
    std::vector<std::string> storage;
    std::istringstream splitter(joined_args);
    std::string word;
    while (splitter >> word) {
        storage.push_back(word);
    }

    std::vector<char*> argv;
    const std::string program = "calc";
    argv.push_back(const_cast<char*>(program.c_str()));
    for (std::string& arg : storage) {
        argv.push_back(&arg[0]);
    }

    std::ostringstream out_stream;
    std::ostringstream err_stream;
    std::istringstream in("");
    const int status = calc::run(static_cast<int>(argv.size()), argv.data(), in, out_stream, err_stream);
    *out = out_stream.str();
    *err = err_stream.str();
    return status;
}

// ---------------------------------------------------------------------------

void test_arithmetic() {
    section("Arithmetic");
    expect_value("1 + 1", 2.0);
    expect_value("10 - 4", 6.0);
    expect_value("6 * 7", 42.0);
    expect_value("9 / 2", 4.5);
    expect_value("2.5 + 0.25", 2.75);
    expect_value("-3 + 3", 0.0);
    expect_value("--5", 5.0);          // Double unary minus.
    expect_value("+-5", -5.0);
    expect_value("1 + 2 + 3 + 4", 10.0);
}

void test_precedence_and_grouping() {
    section("Precedence and grouping");
    expect_value("3 + 4 * 2", 11.0);        // The headline case from the README.
    expect_value("(3 + 4) * 2", 14.0);
    expect_value("2 + 3 * 4 - 6 / 2", 11.0);
    expect_value("(2 + 3) * (4 - 1)", 15.0);
    expect_value("((1 + 2) * 3) ^ 2", 81.0);
    expect_value("100 / 10 / 2", 5.0);      // Left-associative.
    expect_value("2 * 3 % 4", 2.0);
}

void test_power_and_modulo() {
    section("Power and modulo");
    expect_value("2 ^ 10", 1024.0);
    expect_value("2 ^ 3 ^ 2", 512.0);       // Right-associative: 2^(3^2).
    expect_value("2 ^ -2", 0.25);
    expect_value("-2 ^ 2", -4.0);           // Unary binds looser than ^.
    expect_value("(-2) ^ 2", 4.0);
    expect_value("9 ^ 0.5", 3.0);
    expect_value("7.5 % 2", 1.5);           // fmod, not int truncation.
    expect_value("-7.5 % 2", -1.5);
    expect_value("7 % 3", 1.0);
}

void test_numbers() {
    section("Number formats");
    expect_value("1e3", 1000.0);
    expect_value("1.5e2", 150.0);
    expect_value("1E-2", 0.01);
    expect_value("2e+1", 20.0);
    expect_value(".5 + .5", 1.0);
    expect_value("0.0001", 0.0001);
    expect_value("  42  ", 42.0);           // Surrounding whitespace.
}

void test_functions_and_constants() {
    section("Functions and constants");
    expect_value("sqrt(16)", 4.0);
    expect_value("SQRT(16)", 4.0);          // Names are case-insensitive.
    expect_value("sqrt(2 + 14)", 4.0);
    expect_value("abs(-4.5)", 4.5);
    expect_value("floor(2.9)", 2.0);
    expect_value("ceil(2.1)", 3.0);
    expect_value("round(2.5)", 3.0);
    expect_value("exp(0)", 1.0);
    expect_value("ln(1)", 0.0);
    expect_value("log(1000)", 3.0);
    expect_value("sin(0)", 0.0);
    expect_value("cos(0)", 1.0);
    expect_value("pi", 3.14159265358979323846);
    expect_value("e", 2.71828182845904523536);
    expect_value("2 * pi", 6.283185307179586);
    expect_value("sqrt(abs(-9))", 3.0);     // Nested calls.
}

void test_errors() {
    section("Error handling");
    expect_error("1 / 0", "division by zero");
    expect_error("1 / (2 - 2)", "division by zero");
    expect_error("5 % 0", "modulo by zero");
    expect_error("sqrt(-1)", "non-negative");
    expect_error("ln(0)", "positive");
    expect_error("log(-5)", "positive");
    expect_error("1e308 * 10", "too large");
    expect_error("2 & 3", "unexpected character");
    expect_error("(1 + 2", "missing ')'");
    expect_error("1 + 2)", "unexpected");
    expect_error("3 4", "did you forget an operator");
    expect_error("1 +", "where a value was expected");
    expect_error("* 5", "where a value was expected");
    expect_error("", "nothing to calculate");
    expect_error("   ", "nothing to calculate");
    expect_error("1.2.3", "not a valid number");
    expect_error(".", "not a valid number");
    expect_error("foo", "not a known name");
    expect_error("foo(2)", "not a known function");
    expect_error("sqrt(4", "missing ')'");
}

void test_formatting() {
    section("Result formatting");
    expect_equal("integer prints without decimals", calc::format_number(3.0), "3");
    expect_equal("0.1 + 0.2 is not 0.30000000000000004", calc::format_number(0.1 + 0.2), "0.3");
    expect_equal("thirds get 15 significant digits", calc::format_number(1.0 / 3.0),
                 "0.333333333333333");
    expect_equal("negatives keep their sign", calc::format_number(-0.5), "-0.5");
    expect_equal("infinity is spelled out", calc::format_number(
                     std::numeric_limits<double>::infinity()),
                 "infinity");
    expect_equal("NaN is spelled out", calc::format_number(std::nan("")), "undefined");
}

void test_repl() {
    section("Interactive loop");

    {   // A result, then quit.
        std::istringstream in("3 + 4 * 2\nquit\n");
        std::ostringstream out;
        calc::run_repl(in, out);
        expect_output("evaluates a line", out.str(), "= 11");
        expect_output("says goodbye", out.str(), "Thanks for using the calculator. Goodbye!");
    }

    {   // The v1 bug: garbage input must not wedge the loop.
        std::istringstream in("hello\n1 + 1\nq\n");
        std::ostringstream out;
        calc::run_repl(in, out);
        expect_output("bad line reports an error", out.str(), "error:");
        expect_output("loop survives the bad line", out.str(), "= 2");
    }

    {   // End of input exits cleanly instead of looping forever.
        std::istringstream in("1 + 1\n");
        std::ostringstream out;
        calc::run_repl(in, out);
        expect_output("EOF still prints the result", out.str(), "= 2");
        expect_output("EOF still says goodbye", out.str(), "Goodbye!");
    }

    {   // Blank lines are ignored rather than treated as errors.
        std::istringstream in("\n\n  \n5 * 5\nexit\n");
        std::ostringstream out;
        calc::run_repl(in, out);
        expect_output("blank lines are skipped", out.str(), "= 25");
        expect_true("blank lines produce no error", out.str().find("error:") == std::string::npos);
    }

    {   // 'help' prints the reference.
        std::istringstream in("help\nquit\n");
        std::ostringstream out;
        calc::run_repl(in, out);
        expect_output("help lists the operators", out.str(), "Operators:");
        expect_output("help lists the functions", out.str(), "sqrt");
    }

    {   // 'exit' quits too, and the comparison is case-insensitive.
        expect_true("QUIT quits", calc::is_quit_command("QUIT"));
        expect_true("q quits", calc::is_quit_command("q"));
        expect_true("bye quits", calc::is_quit_command("bye"));
        expect_true("calculator does not quit", !calc::is_quit_command("1 + 1"));
    }
}

void test_command_line() {
    section("Command line");

    {
        std::string out;
        std::string err;
        const int status = run_cli("3 + 4 * 2", &out, &err);
        expect_equal("unquoted expression exits 0", std::to_string(status), "0");
        expect_equal("unquoted expression prints the result", out, "11\n");
    }

    {
        std::string out;
        std::string err;
        const int status = run_cli("--bogus", &out, &err);
        expect_equal("unknown option exits 2", std::to_string(status), "2");
        expect_output("unknown option explains itself", err, "unknown option");
    }

    {
        std::string out;
        std::string err;
        const int status = run_cli("1 / 0", &out, &err);
        expect_equal("evaluation error exits 1", std::to_string(status), "1");
        expect_output("evaluation error goes to stderr", err, "division by zero");
    }

    {
        std::string out;
        std::string err;
        const int status = run_cli("--version", &out, &err);
        expect_equal("--version exits 0", std::to_string(status), "0");
        expect_output("--version names the program", out, "Simple Calculator v");
    }

    {   // A leading minus sign is a number, not an option.
        std::string out;
        std::string err;
        const int status = run_cli("-5 + 1", &out, &err);
        expect_equal("negative literal exits 0", std::to_string(status), "0");
        expect_equal("negative literal evaluates", out, "-4\n");
    }

    {   // A negative fraction must not be mistaken for an option either.
        std::string out;
        std::string err;
        const int status = run_cli("-.5 * 4", &out, &err);
        expect_true("negative fraction is not read as an option",
                    err.find("unknown option") == std::string::npos);
        expect_equal("negative fraction exits 0", std::to_string(status), "0");
        expect_equal("negative fraction evaluates", out, "-2\n");
    }

    {   // The option/number distinction, stated directly.
        expect_true("-h is an option", calc::looks_like_option("-h"));
        expect_true("--verbose is an option", calc::looks_like_option("--verbose"));
        expect_true("-5 is a number", !calc::looks_like_option("-5"));
        expect_true("-.5 is a number", !calc::looks_like_option("-.5"));
        expect_true("a bare dash is not an option", !calc::looks_like_option("-"));
        expect_true("5 is not an option", !calc::looks_like_option("5"));
    }
}

}  // namespace

int main() {
    std::cout << "Simple Calculator -- test suite\n";

    test_arithmetic();
    test_precedence_and_grouping();
    test_power_and_modulo();
    test_numbers();
    test_functions_and_constants();
    test_errors();
    test_formatting();
    test_repl();
    test_command_line();

    std::cout << "\n---------------------------------------------\n";
    if (g_failures == 0) {
        std::cout << "All " << g_checks << " checks passed.\n";
        return 0;
    }
    std::cout << g_failures << " of " << (g_checks + g_failures) << " checks FAILED.\n";
    return 1;
}
