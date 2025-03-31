#include "search.h"

#include <ctime>
#include <thread>

chess::Move think(const chess::Board& input_board,
                  const uci::GoArgs& args,
                  const uci::StopSignal& must_stop) {
    // The following code contains a demonstration of how to
    // use GoArgs, StopSignal and report_info.
    //
    // You can (and should) delete this when starting to write
    // your own code.
    //
    // It demonstrates a pseudo search that selects a random move,
    // switches its choice every second -- all while doing UCI reports,
    // taking remaining time in consideration and stopping early when
    // requested.
    //
    // TODO: Replace with your actual search function.

    // Step 1. We need to know how much time we'll spend searching.
    // This depends on the time control the user requested and how
    // much time we have left.
    std::int64_t target_time = INT64_MAX; // Initialize with infinite time.

    if (args.move_time) {
        target_time = *args.move_time - 50;
    }
    else if (args.w_time && input_board.sideToMove() == chess::Color::WHITE) {
        std::int64_t remaining_time = *args.w_time;
        std::int64_t increment = args.w_inc.value_or(0);

        target_time = (remaining_time / 15) + increment;
    }
    else if (args.b_time && input_board.sideToMove() == chess::Color::BLACK) {
        std::int64_t remaining_time = *args.b_time;
        std::int64_t increment = args.b_inc.value_or(0);

        target_time = (remaining_time / 15) + increment;
    }

    // Step 2. Start searching for the best move.
    std::srand(std::time(nullptr));

    chess::Movelist legal_moves;
    chess::movegen::legalmoves(legal_moves, input_board);
    chess::Move best_move = legal_moves[std::rand() % legal_moves.size()];
    int depth = 0;

    while (!must_stop()) {
        // Simulate a search by sleeping for a short duration.
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // Increment depth and select a random move as the best move.
        depth++;
        chess::movegen::legalmoves(legal_moves, input_board);
        if (legal_moves.empty()) {
            break; // No legal moves, game over.
        }
        best_move = legal_moves[std::rand() % legal_moves.size()];

        // Report search progress to the UCI interface.
        uci::report_info(
            uci::info::Depth(depth),
            uci::info::Score((std::rand() % 4000) - 2000, 1000, 10),
            uci::info::Nodes(depth * 1000), // Simulated node count.
            uci::info::PV(&best_move, &best_move + 1, [](const auto& move) { return chess::uci::moveToUci(move); })
        );

        // Stop searching if we've reached the target time.
        if (depth * 1000 >= target_time) {
            break;
        }
    }

    return best_move;
}