#include "engine.h"

#include "search.h"
#include "../ext/libuci/uci.h"

void Engine::initialize(int argc, char* argv[]) {

    // Set up the 'uci' command with your engine name and your own name.
    uci::register_uci("My Engine Name", "My Name");

    // Set up 'setoption'.
    uci::register_setoption();

    // Some tools, like OpenBench, require Threads and Hash to be exposed
    // as options. We can expose them even if we don't use them.
    uci::register_spin_option("Threads", 1, 1, 1);
    uci::register_spin_option("Hash", 32, 1, 1024 * 1024);

    // Set up 'ucinewgame'.
    uci::register_ucinewgame([]() {
        // TODO: Clear anything that shouldn't be kept from game to game here.
    });

    // Set up 'stop'.
    uci::register_stop([&]() {
        uci::stop_work_thread();
    });

    // Set up 'position'.
    uci::register_position([&](const uci::PositionArgs& args) {
        m_board = chess::Board(args.fen);

        for (const auto& move: args.moves) {
            m_board.makeMove(chess::uci::uciToMove(m_board, move));
        }
    });

    // Set up 'go'.
    uci::register_go([&](const uci::GoArgs& args) {
        uci::launch_work_thread([=](const uci::StopSignal& must_stop) {
            chess::Move best_move = think(m_board, args, must_stop);
            uci::report_best_move(chess::uci::moveToUci(best_move));
        });
    });

    // In order to support OpenBench, engines need to be enable "benching"
    // from command line args.
    uci::register_custom_command("bench", [&](const uci::CommandContext& ctx) {
       bench();
    });
    if (argc > 1 && argv[1] == std::string("bench")) {
        bench();
    }

    // Set up other trivial UCI commands.
    uci::register_isready();
    uci::register_quit();
}

void Engine::bench() {
    // TODO: Replace with a proper bench implementation.
    std::cout << "2000 nodes 2000 nps" << std::endl;
}