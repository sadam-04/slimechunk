#include "SafeServerState.hpp"

#include <string>

class Wrapper {
private:
    unsigned int id;
    std::string workingDir;
    SafeServerState svrState;

    std::mutex writeMtx;
    int server_stdin_fd;
    int server_stdout_fd;

    int childPid;

    //int shutdownFd;

    unsigned int _get_id();

    ssize_t _safe_send(const char* line);
public:
    Wrapper(std::string workingDir);
    
    // opens server_stdin_fd and server_stdout_fd
    int start();
    int stop();

    // int print_and_cleanup();
    // int collect_input();

    int interact();

    // bool get_svr_out_line(std::string& line, int fd);
};