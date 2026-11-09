#include "wrapper.hpp"
#include "util.hpp"

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

int Wrapper::start() {
    svrState_t curState = svrState.get();
    if (curState == svrState_t::starting) {
        // std::cerr << "[Server-" << id << "]:"
        std::cerr << "Server is already starting\n";
        return 1;
    }
    if (curState == svrState_t::running) {
        std::cerr << "Server is already running\n";
        return 1;
    }

    int parentToChild[2];
    int childToParent[2];

    // int parentToChild[2];
    // int childToParent[2];
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
        
        close(parentToChild[0]);
        close(parentToChild[1]);
        close(childToParent[0]);
        close(childToParent[1]);
        
        const char* cmd = "/usr/bin/java";
        
        if (!scutils::dir_exists(workingDir.c_str())) {
            mode_t dirMode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH; // rwx for user and group, rx for others
            if (mkdir(workingDir.c_str(), dirMode) < 0) {
                std::cerr << "Failed: mkdir(" << workingDir << ")\n";
                std::cerr << strerror(errno) << '\n';
                _exit(1);
            }
        }

        // cd into the specified working directory
        svrState.set(svrState_t::starting);
        if (chdir(workingDir.c_str()) < 0) {
            std::cerr << "Failed: chdir(" << workingDir << ")\n";
            _exit(1);
        }

        // dangerous casting const strings to non-const but should be fine as-is. execvp guarantees never modifying args
        char* args[] = { const_cast<char*>(cmd), const_cast<char*>("-jar"), const_cast<char*>("server.jar"), const_cast<char*>("-nogui"), nullptr };
        execvp(cmd, args);

        std::cerr << "Failed: execve()\n";
        _exit(1); //should never reach here once execvp replaces this process with itself unless something goes wrong
    } else { // PARENT
        childPid = pid;

        close(parentToChild[0]);
        close(childToParent[1]);

        server_stdin_fd = parentToChild[1];
        server_stdout_fd = childToParent[0];

        return 0;

        // pid = fork();

        // if (pid < 0) {
        //     std::cerr << "inner fork() failed\n";
        //     return 1;
        // } else if (pid == 0) { // CHILD 2 (the cleanup one)
        //     std::cout << "CHILD 2 BORN\n";
        //     int status;
        //     waitpid(childPid, &status, 0);
        //     std::cout << "CHILD 2 AWAKE\n";

        //     uint64_t shutdownSignal = 1;
        //     write(shutdownFd, &shutdownSignal, sizeof(shutdownSignal));

        //     close(server_stdin_fd);
        //     close(server_stdout_fd);
            
        //     _exit(0);
        // } else { // PARENT
        //     std::cout << "i am the parent\n";
        //     return 0;
        // }
    }
}

// int Wrapper::stop() {

// }

int Wrapper::interact() {
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

        // // 1. Check if the main program signaled us to shut down
        // if (fds[2].revents & POLLIN) {
        //     running = false; 
        //     break;
        // }

        if (fds[2].revents & POLLIN) {
            int status;
            // The FD triggers when the process dies, but you still MUST call 
            // waitpid to reap the zombie process from the OS process table.
            // This call will return instantly because we already know it's dead.
            waitpid(childPid, &status, 0);
            
            // Perform your cleanup, restart the server, or exit the loop
            close(pidfd);
            running = false;
            break;
        }

        // 2. Check if the user typed something
        if (fds[0].revents & POLLIN) {
            char buffer[1024];
            ssize_t bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer)-2);
            if (bytes_read > 0) {
                // Write this buffer to your server_stdin_pipe
                // write(server_stdin_pipe, buffer, bytes_read);
                buffer[bytes_read] = '\n';
                buffer[bytes_read+1] = '\0';
                _safe_send(buffer);

            }
        }

        // 3. Check if the Minecraft server output something
        if (fds[1].revents & POLLIN) {
            char buffer[4096];
            ssize_t bytes_read = read(server_stdout_fd, buffer, sizeof(buffer));
            if (bytes_read > 0) {
                // Print it to the console, or log it
                // write(STDOUT_FILENO, buffer, bytes_read);
                std::cout.write(buffer, bytes_read);
                std::cout.flush();
            }
        }
    }

    return 0;
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

//     return scutils::read_line_fd(line, fd);
// }