#pragma once

#include "SafeServerState.hpp"

#include <string>
#include <thread>

class Wrapper {
private:
    unsigned int id;
    std::string workingDir;
    SafeServerState svrState;
    bool restartOnExit;

    std::mutex writeMtx;
    int server_stdin_fd;
    int server_stdout_fd;

    int childPid;

    std::thread cleanupWorker; 

    //int shutdownFd;

    unsigned int _get_id();
    int _launch_process();
    void _cleanup();
    ssize_t _safe_send(const char* line);
public:
    Wrapper(std::string workingDir);
    
    // opens server_stdin_fd and server_stdout_fd
    int start();
    int stop();

    void setRestartPolicy(bool policy);
    svrState_t get_state();

    // int print_and_cleanup();
    // int collect_input();


    // should be able to return instantly when the mode switch command is entereed, and should also return when the server exits
    int interact();

    // bool get_svr_out_line(std::string& line, int fd);
};