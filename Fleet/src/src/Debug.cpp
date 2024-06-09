// Debug.cpp
#include "Debug.h"

// ANSI color codes
const std::string GREEN = "\033[32m";
const std::string RED = "\033[31m";
const std::string RESET = "\033[0m";

void Debug::log(const std::string& message) {
    std::cout << GREEN << "Log: " << message << RESET << std::endl;
}

void Debug::error(const std::string& message) {
    std::cerr << RED << "Error: " << message << RESET << std::endl;
}
