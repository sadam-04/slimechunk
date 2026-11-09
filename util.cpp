#include "util.hpp"

#include <unistd.h>

bool scutils::read_line_fd(std::string& line, int fd, bool includeNewline) {
    line.clear();
    char ch;
    ssize_t bytes_read;

    while ((bytes_read = read(fd, &ch, 1)) > 0) {
        if (ch == '\n') {
            if (includeNewline)
                line += '\n';
            return true;
        }
        line += ch;
    }

    return !line.empty();
}

int scutils::dir_exists(const char *path) {
    struct stat stats;
    
    // stat() returns 0 if the path exists
    if (stat(path, &stats) == 0) {
        // Check if the path is a directory
        return S_ISDIR(stats.st_mode);
    }
    
    return 0; // Does not exist or error
}