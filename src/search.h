#ifndef SEARCH_H
#define SEARCH_H

#include "../ext/chess/chess.h"
#include "../ext/libuci/uci.h"

chess::Move think(const chess::Board& board,
                  const uci::GoArgs& args,
                  const uci::StopSignal& must_stop);

#endif //SEARCH_H
