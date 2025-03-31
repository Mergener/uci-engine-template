#include "../ext/libuci/uci.h"

#include "engine.h"

int main(int argc, char* argv[]) {
    Engine e {};
    e.initialize(argc, argv);
    uci::main_loop();
    return 0;
}