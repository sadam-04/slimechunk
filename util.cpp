#include "util.hpp"
#include "macros.hpp"

#include <unistd.h>
#include <cstring>
#include <iostream>
// #include <string>

int sadamtools::read_line_fd(char *buffer, int fd, int maxReadSize) {
    // buffer.clear();
    char ch;
    ssize_t bytes_read = 0;

    while ((bytes_read += read(fd, &ch, 1)) > 0) {
        buffer[bytes_read] = ch;
        
        if (ch == '\n') {
            // std::cout << "[\\n detected]";
            return bytes_read;
        }
        if (bytes_read == maxReadSize) break;
    }

    return bytes_read;
}

int sadamtools::dir_exists(const char *path) {
    struct stat stats;
    
    // stat() returns 0 if the path exists
    if (stat(path, &stats) == 0) {
        // Check if the path is a directory
        return S_ISDIR(stats.st_mode);
    }
    
    return 0; // Does not exist or error
}

bool sadamtools::isYo(std::string line) {
    return sadamtools::cisYo(line.c_str());
}

bool sadamtools::cisYo(const char *line) {
    int n = std::strlen(line);
    if (n <= 1) return false;
    if (line[0] != 'y' && line[0] != 'Y') return false;

    for (int i = 1; i < n; i++) {
        if (line[i] == '\n' && line[i+1] == '\0') return true;
        if (line[i] != 'o' && line[i] != 'O') {
            return false;
        }
    }

    return true;
}

void sadamtools::swarn(std::string line) {
    if (line.back() == '\n') {
        line = line.substr(0, line.size()-1);
    }
    std::cout << "\033[200;32;32m" << line << "\033[0m" << '\n';
}

void sadamtools::serror(std::string line) {

}
void sadamtools::slog(std::string line) {
    if (line.back() == '\n') {
        line = line.substr(0, line.size()-1);
    }
    // std::cout << "\033[38;5;206;48;5;57m" << line << "\033[K\033[0m\n";
    std::cout << "\033[38;5;255;255;255m" << line << '\n';
}

void setCraftColors() {
    std::cout << "\033[38;2;" << CRAFT_FG_COLOR << "m";
    // std::cout << "\033[48;2;" << CRAFT_BG_COLOR << "m";
}

void setSlimeColors() {
    std::cout << "\033[38;2;" << SC_FG_COLOR << "m";
    // std::cout << "\033[48;2;" << SC_BG_COLOR << "m";
}

void fillLineBg() {
    std::cout << "\033[K";
}

void resetColors() {
    std::cout << "\033[0m";
}