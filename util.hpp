#pragma once

#include <string>
#include <sys/stat.h>

// namespace scutils {
    // }
    
namespace sadamtools {
    int read_line_fd(char *buffer, int fd, int maxReadSize);
    int dir_exists(const char *path);
    bool isYo(std::string line);
    bool cisYo(const char *line);

    void swarn(std::string line);
    void serror(std::string line);
    void slog(std::string line);
}

void setCraftColors();

void setSlimeColors();

void fillLineBg();

void resetColors();