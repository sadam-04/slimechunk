#include <string>
#include <sys/stat.h>

namespace scutils {
    bool read_line_fd(std::string& line, int fd, bool includeNewline = false);
    int dir_exists(const char *path);
}