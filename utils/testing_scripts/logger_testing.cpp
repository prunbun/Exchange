#include "../logger.h"

int main() {
    using namespace Common;

    // sample data we want to log

    char c = 'a';
    int i = 3;
    unsigned long ul = 65;
    float f = 3.14;
    double d = 100.1;
    const char* c_string = "testing c-style strings";
    std::string cpp_string = "testing cpp-style strings";

    // create a logger on the stack
    // note that if we allocated heap memory here, then, any member variables would also be allocated on the heap...
    Logger logger("logging_example.log");

    // try logging small arithmetic types
    logger.log("Logging a char: %, an int: %, and a ul: % \n", c, i, ul);

    // larger arithmetic types
    logger.log("Logging a float: %, and a double: % \n", f, d);

    // strings
    logger.log("trying to log a c-style string: % \n", c_string);
    logger.log("trying to log a cpp-style string: % \n", cpp_string);

    return 0;
}