#ifndef ENGINE_H
#define ENGINE_H

#include <atomic>

#include "../ext/chess/chess.h"

class Engine {
public:
    void initialize(int argc, char* argv[]);
    static void bench();

private:
    chess::Board m_board {};
    std::atomic_bool m_should_stop_search {};
};


#endif //ENGINE_H
