#include "Verification.h"
#include <iostream>

int main() {
    Verification verification;

    bool test1 = verification.testNumber("1248493027", "I8");
    bool test2 = !verification.testNumber("128", "I8");
    bool test3 = verification.testNumber("255", "U8");
    bool test4 = !verification.testNumber("2856", "U8");
    bool test5 = verification.testNumber("32767", "I16");
    bool test6 = !verification.testNumber("32768", "I16");

    std::cout << "I8 maximum: " << (test1 ? "PASS" : "FAIL") << '\n';
    std::cout << "I8 overflow: " << (test2 ? "PASS" : "FAIL") << '\n';
    std::cout << "U8 maximum: " << (test3 ? "PASS" : "FAIL") << '\n';
    std::cout << "U8 overflow: " << (test4 ? "PASS" : "FAIL") << '\n';
    std::cout << "I16 maximum: " << (test5 ? "PASS" : "FAIL") << '\n';
    std::cout << "I16 overflow: " << (test6 ? "PASS" : "FAIL") << '\n';

    return (
        test1 && test2 && test3 &&
        test4 && test5 && test6
    ) ? 0 : 1;
}