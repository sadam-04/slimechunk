#include <mutex>

enum class svrState_t {
    stopped,
    starting,
    running,
    stopping
};

class SafeServerState {
private:
    svrState_t _state;
    mutable std::mutex _mutex;
public:
    SafeServerState(svrState_t state) : _state(state) {};

    svrState_t get() const;
    void set(svrState_t state);
};