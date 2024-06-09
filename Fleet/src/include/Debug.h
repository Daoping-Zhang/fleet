// Debug.h
#ifndef DEBUG_H
#define DEBUG_H

#include <iostream>
#include <string>

/**
 * @file Debug.h
 * @brief Debug class for logging and error reporting.
 *
 * This class provides static methods for logging general information and error messages.
 * It is designed to be simple and straightforward for use in small projects or for learning purposes.
 */
class Debug {
public:
    // Logs a general message
    static void log(const std::string& message);

    // Logs an error message
    static void error(const std::string& message);
};

#endif // DEBUG_H
