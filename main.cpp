#include <iostream>
using namespace std;

int main() {
    char operation;
    double num1, num2;
    char choice;

    do {
        // Input
        cout << "\n=== SIMPLE CALCULATOR ===\n";
        cout << "Enter first number: ";
        cin >> num1;

        cout << "Enter an operator (+, -, *, /, %): ";
        cin >> operation;

        cout << "Enter second number: ";
        cin >> num2;

        // Calculation
        switch (operation) {
            case '+':
                cout << "Result: " << num1 + num2 << endl;
                break;
            case '-':
                cout << "Result: " << num1 - num2 << endl;
                break;
            case '*':
                cout << "Result: " << num1 * num2 << endl;
                break;
            case '/':
                if (num2 != 0)
                    cout << "Result: " << num1 / num2 << endl;
                else
                    cout << "Error: Division by zero is not allowed." << endl;
                break;
            case '%':
                if (static_cast<int>(num2) != 0)
                    cout << "Result: " << static_cast<int>(num1) % static_cast<int>(num2) << endl;
                else
                    cout << "Error: Modulo by zero is not allowed." << endl;
                break;
            default:
                cout << "Invalid operator!" << endl;
        }

        // Repeat?
        cout << "Do you want to calculate again? (y/n): ";
        cin >> choice;
    } while (choice == 'y' || choice == 'Y');

    cout << "Thanks for using the calculator. Goodbye!\n";
    return 0;
}
