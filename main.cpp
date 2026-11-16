#include "wrapper.hpp"
#include "macros.hpp"
#include "util.hpp"

#include <string>
#include <iostream>
#include <thread>

int main() {
    std::string serverDir("mc");
    Wrapper server(serverDir);

    // setSlimeColors();
    // fillLineBg();
    resetColors();

    // int i = 0;
    // while (++i < 3) {
    //     server.start(); // non blocking
    //     server.interact(); // blocking, returns when server exits
    // }

    bool shouldExit = false;
    while (!shouldExit) {
        std::string line;
        std::cout << "\033[38;2;" << "255;64;0" << "myo\033[38;2;" << "0;0;255" << "m\u6211\u611b\u4f60\033[38;2;255;255;255m$ ";
        resetColors(); // restore state after mangling it in the above line

        std::getline(std::cin, line);

        if (line == "help") {
            std::cout << "no :)\n";
        } else if (line == "start") {
            server.start();
        } else if (line == SWITCH_IN_CMD) {
            server.interact();
            // setSlimeColors();
        } else if (line == "restart on") {
            server.setRestartPolicy(true);
            std::cout << "enabled auto-restart\n";
        } else if (line == "restart off") {
            server.setRestartPolicy(false);
            std::cout << "disabled auto-restart\n";
        } else if (line == "go") {
            server.start();
            server.interact();
            // setSlimeColors();
        } else if (sadamtools::isYo(line) == true) {
            sadamtools::slog("YO");
        } else if (line == "exit") {
            if (server.get_state() == svrState_t::stopped) {
                shouldExit = true;
            } else {
                std::cout << "Cannot exit while server is running\n";
            }
        } else if (line == "hi") {
            std::cout << "hello\n";
        }
    }

    // DESIGN THE INTERFACE BEFORE ANY MORE IMPLEMENTATION

    // interface should be like a shell, text based
    // has two "modes":
    //      1. interacting with slimechunk
    //      2. interacting with mc server
    // there will be some command that toggles between the two.
    // that means interact() must be able to exit based on getline() contents
    // ALSO means interact() must not be responsible for cleaning up the
    //   server. the server may stop while user is in mode 1. so the start()
    //   method should spawn a worker thread that waits on the server to exit
    //   then runs cleanup. that should set the Wrapper state so that
    //   interact() returns instantly, and exits back to mode 1 is mode 2 is
    //   active.

    std::cout << "otay bye aga\n";

    return 0;
}