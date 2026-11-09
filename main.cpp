#include "wrapper.hpp"

#include <string>
#include <iostream>
#include <thread>

int main() {
    std::string serverDir("/home/adam/repos/slimechunk/mc");
    Wrapper server(serverDir);

    int i = 0;
    while (++i < 3) {
        server.start(); // non blocking
        server.interact(); // blocking, returns when server exits
    }
    

    std::cout << "otay bye aga\n";

    return 0;
}