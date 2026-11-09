#include "SafeServerState.hpp"

svrState_t SafeServerState::get() const {
    std::lock_guard<std::mutex> guard(_mutex);
    return _state;
}

void SafeServerState::set(svrState_t state) {
    std::lock_guard<std::mutex> guard(_mutex);
    _state = state;
}