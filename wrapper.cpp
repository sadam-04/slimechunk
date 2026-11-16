#include "wrapper.hpp"
#include "util.hpp"
#include "macros.hpp"

#include <unistd.h>
#include <iostream>
#include <chrono>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <cstring>

Wrapper::Wrapper(std::string workingDir) : workingDir(workingDir), svrState(svrState_t::stopped) {
    // this->workingDir = workingDir;
    svrState.set(svrState_t::stopped);
    id = _get_id();

    server_stdin_fd = -1;
    server_stdout_fd = -1;
}

unsigned int Wrapper::_get_id() {
    static unsigned int NEXT_ID = 0;
    return NEXT_ID++;
}

int Wrapper::_launch_process() {
    int parentToChild[2];
    int childToParent[2];

    if (pipe(parentToChild) < 0 || pipe(childToParent) < 0) {
        std::cerr << "Pipe creation failed\n";
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Fork failed\n";
        return 1;
    }

    if (pid == 0) { // CHILD
        dup2(parentToChild[0], STDIN_FILENO);
        dup2(childToParent[1], STDOUT_FILENO);
        dup2(STDOUT_FILENO, STDERR_FILENO);

        close(parentToChild[0]);
        close(parentToChild[1]);
        close(childToParent[0]);
        close(childToParent[1]);
        
        const char* cmd = "/usr/bin/java";
        
        if (!sadamtools::dir_exists(workingDir.c_str())) {
            mode_t dirMode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
            if (mkdir(workingDir.c_str(), dirMode) < 0) {
                std::cerr << "Failed: mkdir(" << workingDir << ")\n";
                _exit(1);
            }
        }

        if (chdir(workingDir.c_str()) < 0) {
            std::cerr << "Failed: chdir(" << workingDir << ")\n";
            _exit(1);
        }

        char* args[] = { const_cast<char*>(cmd), const_cast<char*>("-jar"), const_cast<char*>("server.jar"), const_cast<char*>("-nogui"), nullptr };
        execvp(cmd, args);

        std::cerr << "Failed: execvp()\n";
        _exit(1); 
    } else { // PARENT
        childPid = pid;

        close(parentToChild[0]);
        close(childToParent[1]);

        server_stdin_fd = parentToChild[1];
        server_stdout_fd = childToParent[0];

        return 0;
    }
}

int Wrapper::start() {
    svrState_t curState = svrState.get();
    if (curState == svrState_t::starting || curState == svrState_t::running) {
        std::cerr << "Server is already starting or running\n";
        return 1;
    }

    if (cleanupWorker.joinable()) {
        cleanupWorker.join();
    }

    svrState.set(svrState_t::starting);

    if (_launch_process() != 0) {
        svrState.set(svrState_t::stopped);
        return 1;
    }

    svrState.set(svrState_t::running);
    cleanupWorker = std::thread(&Wrapper::_cleanup, this);

    return 0;
}

// int Wrapper::stop() {

// }

void Wrapper::setRestartPolicy(bool policy) {
    restartOnExit = policy;
}

svrState_t Wrapper::get_state() {
    return svrState.get();
}

int Wrapper::interact() {
    // setCraftColors();
    if (svrState.get() != svrState_t::running) {
        // std::cout << "\033[31mServer is not running\033[0m\n";
        std::cout << "Server is not running\n";
        // std::cout 
        return 0;
    }

    // std::cout << "\033[48;2;" << CRAFT_BG_COLOR << "m"
    // << "\033[38;2;" << CRAFT_FG_COLOR << "m"
    // << "\033[K";
    // std::cout.flush();

    // shutdownFd = eventfd(0, EFD_NONBLOCK);

    struct pollfd fds[3];

    // Watch for user input
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    // Watch for server output
    fds[1].fd = server_stdout_fd;
    fds[1].events = POLLIN;

    // // Watch for shutdown signal
    // fds[2].fd = shutdownFd;
    // fds[2].events = POLLIN;

    int pidfd = syscall(SYS_pidfd_open, childPid, 0);
    fds[2].fd = pidfd;
    fds[2].events = POLLIN;

    bool running = true;

    while (running) {
        // Block until there is activity on ANY of the monitored file descriptors
        int ret = poll(fds, 3, -1); 
        
        if (ret < 0) {
            // Handle error (e.g., check errno for EINTR if interrupted by a signal)
            break;
        }

        // wrapper stop signal
        if (fds[2].revents & POLLIN) {
            close(pidfd);
            running = false;
            // std::cout << "\n\033[0m\033[K";
            break;
        }

        // user input
        if (fds[0].revents & POLLIN) {
            char buffer[1024];
            ssize_t bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer)-1);
            if (bytes_read > 0) {
                // buffer[bytes_read] = '\n';
                buffer[bytes_read] = '\0';
                
                // if (std::strncmp(buffer, "switch\n", 7) == 0) {
                if (std::string(SWITCH_OUT_CMD) + "\n" == buffer) {
                    // std::cout << "\033[0m\033[K";
                    running = false;
                    break;
                } else {
                    // std::cout << "not switch command. got [" << buffer << "]";
                    _safe_send(buffer);
                }
            }
        }

        // server output
        if (fds[1].revents & POLLIN) {
            char buffer[4096];
            ssize_t bytes_read = read(server_stdout_fd, buffer, sizeof(buffer) - 1);
            // ssize_t bytes_read = sadamtools::read_line_fd(buffer, server_stdout_fd, sizeof(buffer)); // BAD BECAUSE WILL BLOCK IF THE SERVER EVER WRITES A NON-NEWLINE TERMINATED LINE TO STDOUT
            if (bytes_read > 0) {
                // Print it to the console, or log it
                // write(STDOUT_FILENO, buffer, bytes_read);
                // std::cout << "\033[K";
                std::cout.write(buffer, bytes_read);
                // setCraftColors();
                std::cout.flush();
            }
        }
    }

    return 0;
}

void Wrapper::_cleanup() {
    while (true) {
        int status;
        waitpid(childPid, &status, 0);

        close(server_stdin_fd);
        close(server_stdout_fd);
        server_stdin_fd = -1;
        server_stdout_fd = -1;

        if (!restartOnExit) {
            break; // No restart requested, break loop and end thread
        }
        
        svrState.set(svrState_t::starting);

        if (_launch_process() != 0) {
            std::cerr << "failed to launch server process\n";
            break; 
        }

        svrState.set(svrState_t::running);
    }

    svrState.set(svrState_t::stopped);
}

ssize_t Wrapper::_safe_send(const char* line) {
    std::lock_guard<std::mutex> lock(writeMtx);
    // std::cout << "_safe_send: sending\n";
    int res = write(server_stdin_fd, line, std::strlen(line));
    // std::cout << "_safe_send: sent! res=" << res << "\n";
    return res;
}

// int Wrapper::print_and_cleanup() {
//     char buffer[256];
//     ssize_t bytes_read;

//     while ((bytes_read = read(server_stdout_fd, buffer, sizeof(buffer) - 1)) > 0) {
//         buffer[bytes_read] = '\0';
//         std::cout << buffer;
//     }

//     // svrState.set(svrState_t::stopping);

//     int status;
//     waitpid(childPid, &status, 0);

//     // cleanup after server stops
//     std::cout << "closing fds\n";
//     close(server_stdin_fd);
//     close(server_stdout_fd);

//     svrState.set(svrState_t::stopped);

//     std::cout << "ok closed aga\n";
//     return 0;
// }

// int Wrapper::collect_input() {
//     while (true) {
//         std::string line;
//         std::cout << "getting line:";
//         if (!std::getline(std::cin, line)) break;
//         std::cout << "yeah we still here\n";
//         line += '\n';
//         // std::cout << "sending line=(" << line << ")\n";
//         _safe_send(line);
//     }
//     std::cout << "stopping the icollector :)\n";
//     return 0;
// }


// void Wrapper::print_and_exit() {
//     char buffer[256];
//     ssize_t bytes_read;

//     while ((bytes_read = read(server_stdout_fd, buffer, sizeof(buffer) - 1)) > 0) {
//         buffer[bytes_read] = '\0';
//         std::cout << buffer;
//     }


// }

// bool Wrapper::get_svr_out_line(std::string& line, int fd) {
//     // char buffer[256];
//     // ssize_t bytes_read;

//     // while ((bytes_read = read(server_stdout_fd, buffer, sizeof(buffer) - 1)) > 0) {
//     //     buffer[bytes_read] = '\0';
//     //     std::cout << buffer;
//     // }

//     return sadamtools::read_line_fd(line, fd);
// }