#pragma once

#include <string>

// ISBN is a textual identifier: leading zeroes and separators are preserved.
struct Book {
    std::string isbn;
    std::string title;
    std::string author;
    bool isAvailable;
};
