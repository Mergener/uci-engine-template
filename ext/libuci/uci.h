//
// MIT License
//
// Copyright (c) 2025 Thomas Mergener
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#ifndef UCI_H
#define UCI_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <iostream>
#include <utility>
#include <string>
#include <stdexcept>
#include <sstream>
#include <type_traits>
#include <variant>

namespace uci {

//
// Forward Declarations
//
class CommandContext;
struct OptionInfo;
struct GoArgs;
struct PositionArgs;
class InputError;
class ArgReader;
class WorkThread;
//

/**
 * The main UCI process loop.
 * Listens to user input and dispatches it to its respective command.
 */
void main_loop();

/**
 * Exception upon different situations of wrong input.
 * When thrown inside the UCI main loop, it won't reach
 * the error handler nor cause the program to terminate.
 */
class InputError : public std::runtime_error {
public:
    explicit InputError(const std::string& s);
};

/**
 * Returns the value of a 'string' type option.
 * Throws InputError if the option doesn't exist
 * or is not of type string.
 */
const std::string& get_string_option(const std::string& name);

/**
 * Returns the value of a 'spin' type option.
 * Throws InputError if the option doesn't exist
 * or is not of type spin.
 */
std::int64_t get_spin_option(const std::string& name);

/**
 * Returns the value of a 'check' type option.
 * Throws InputError if the option doesn't exist
 * or is not of type check.
 */
bool get_check_option(const std::string& name);

/**
 * Value of a UCI option. The active value varies on
 * the option type:
 *
 *  button: index 0 - std::monostate
 *  spin:   index 1 - std::int64_t
 *  string: index 2 - std::string
 *  check:  index 3 - bool
 *
 */
using OptionValue = std::variant<std::monostate,
                                 std::int64_t,
                                 std::string,
                                 bool>;

/**
 * Sets the value of the specified UCI option.
 */
void set_option(const std::string& name, const OptionValue& value);

/**
 * Triggers a Button UCI option.
 * Shorthand for set_option(name, std::monostate()).
 */
void trigger_button_option(const std::string& name);

/**
 * Returns a vector of all registered options.
 */
std::vector<OptionInfo> get_all_options();

/**
 * Types of options as defined in the UCI spec.
 */
enum class OptionType {
    Button,
    Spin,
    String,
    Check,
};

struct OptionInfo {
    OptionValue current_value;
    std::string name;
    OptionType type;
    OptionValue default_value;

    /** Only available at spin type options. */
    OptionValue min;
    /** Only available at spin type options. */
    OptionValue max;
};

/**
 * Returns information about a UCI option.
 * Throws std::out_of_range if the option is not found.
 */
OptionInfo get_option_info(const std::string& opt_name);

/**
 * Registers a UCI option of type 'button'.
 * Button type options don't hold values, instead are simply
 * triggered by setoption calls.
 */
void register_button_option(const std::string& name,
                            const std::function<void()>& trigger_handler);

/**
 * Registers a UCI option of type 'check' (aka boolean).
 */
void register_check_option(const std::string& name,
                           bool default_value,
                           const std::function<void(bool)>& change_handler = [](bool){});

/**
 * Registers a UCI option of type 'spin' (aka integer).
 */
void register_spin_option(const std::string& name,
                          std::int64_t default_value,
                          std::int64_t min,
                          std::int64_t max,
                          const std::function<void(std::int64_t)>& change_handler = [](std::int64_t){});

/**
 * Registers a UCI option of type 'string'.
 */
void register_string_option(const std::string& name,
                            std::string default_value,
                            const std::function<void(const std::string&)>& change_handler = [](const std::string&){});

/**
 * Registers the 'uci' command with a handler that
 * prints the engine name, author name, available options
 * and uciok.
 */
void register_uci(const std::string& engine_name,
                  const std::string& author_name);

/**
 * Register the 'setoption' command with a handler that simply
 * calls set_option() accordingly.
 */
void register_setoption();

/**
 * Registers the 'quit' command with a handler that
 * calls std::exit(0).
 */
void register_quit();

/**
 * Registers the 'isready' command with a handler that
 * prints 'readyok'.
 */
void register_isready();

/**
 * Registers the 'ucinewgame' command with the specified handler.
 */
void register_ucinewgame(const std::function<void()>&);

/**
 * Registers the 'stop' command with the specified handler.
 */
void register_stop(const std::function<void()>&);

/**
 * Replaces the default exception handler.
 * By default, all exceptions besides InputError are handled
 * by outputting the error to stderr and calling std::terminate().
 */
void set_error_handler(std::function<void(const std::exception& e)> handler);

/**
 * A function that, when true, indicates that the current
 * task must cease operations.
 */
using StopSignal = std::function<bool()>;

/**
 * Launches a work thread that will execute the specified functor.
 * In order not to block the UCI thread while searching, you should run
 * your search method by passing it as a functor to this function.
 */
void launch_work_thread(const std::function<void(StopSignal)>& task);

/**
 * Signals the work thread that it should stop. Blocks the calling thread
 * until the work thread finally stops.
 */
void stop_work_thread();

/**
 * Returns true if the work thread is active.
 */
bool work_thread_running();

/**
 * Creates and awakes the work thread. This is done automatically
 * by launch_work_thread(), but you can do this at engine initialization
 * to speed up first search startup.
 */
void awake_work_thread();

/**
 * Registers the 'go' command with a handler that
 * parses requested limits.
 */
void register_go(const std::function<void(const GoArgs&)>& handler);
struct GoArgs {
    std::optional<int> depth;
    std::optional<std::int64_t> w_time;
    std::optional<std::int64_t> w_inc;
    std::optional<std::int64_t> b_time;
    std::optional<std::int64_t> b_inc;
    std::optional<std::int64_t> move_time;
    std::optional<std::int64_t> nodes;

    /**
     * True if the user sent 'go infinite' or just 'go'.
     */
    bool infinite = true;
};

/**
 * Registers the 'position' command with a handler
 * that extracts startpos/fen and any requested moves.
 * Important: minimal input validation is done here. You still
 * have to validate the FEN and moves yourself.
 */
void register_position(const std::function<void(const PositionArgs&)>& handler);
struct PositionArgs {
    /**
     * The position FEN.
     * If the user specifies startpos, this will be the startpos FEN.
     */
    std::string_view fen;
    std::vector<std::string> moves;
};

/**
 * Registers a custom command. This can also be used to register
 * UCI standard commands.
 * @param command The command identifier.
 * @param handler Handler function for the command.
 */
void register_custom_command(std::string command,
                             std::function<void(CommandContext& ctx)> handler);

/**
 * Generates a info string and reports it.
 * Designed to be used alongside the helpers within uci::info,
 * but you can pass anything that could be passed to an ostream
 * as argument.
 *
 * Example usage:
 *
 * using namespace uci;
 *
 * report_info(info::Depth(10),
 *             info::BestMove("d2d4"),
 *             info::Time(2000));
 *
 * Will output: info depth 10 bestmove e2e4 time 2000
 */
template <typename... TArgs>
void report_info(const TArgs&... args);

/**
 * UCI bestmove output.
 * Prints 'bestmove x' or 'bestmove x ponder y' depending
 * on whether a ponder move was provided.
 */
void report_best_move(const std::string& move_str,
                      const std::string& ponder_move_str = "");

namespace info {

/** string <string> */
struct String {
    std::string str;
    explicit String(std::string str);
};

/**
 * abs(score) < (mate_score - max_mate_plies)
 *  ? cp <score>
 *  : score mate <abs(mate_score - score)>
 */
struct Score {
    int score;
    bool allow_mate_scores;
    int mate_score {};
    int mate_threshold {};

    explicit Score(int score);
    Score(int score,
          int mate_score,
          int max_mate_plies = 1000);
};

/** depth <depth> */
struct Depth {
    int depth;
    explicit Depth(int depth);
};

/** seldepth <sel_depth> */
struct SelDepth {
    int sel_depth;
    explicit SelDepth(int sel_depth);
};

/** hash_full <hash_full> */
struct HashFull {
    int hash_full;
    explicit HashFull(int hash_full);
};

/** tbhits <tb_hits> */
struct TbHits {
    std::uint64_t tb_hits;
    explicit TbHits(std::uint64_t tb_hits);
};

/** multipv <multipv> */
struct Multipv {
    int multipv;
    explicit Multipv(int multipv);
};

/** nodes <nodes> */
struct Nodes {
    std::uint64_t nodes;
    explicit Nodes(std::uint64_t nodes);
};

/** nps <nps> */
struct Nps {
    std::uint64_t nps;
    explicit Nps(std::uint64_t nps);
};

/** time <time> */
struct Time {
    std::uint64_t time;
    explicit Time(std::uint64_t time);
};

/** currmovenumber <curr_move_number> */
struct CurrMoveNumber {
    int curr_move_number;
    explicit CurrMoveNumber(int curr_move_number);
};

/** currmove <move> */
template <typename TMove>
struct CurrMove {
    TMove move;

    explicit CurrMove(TMove move);
};

/** pv *<begin>++ *<begin>++ ... *<end>-1 */
template <typename TIter, typename TMapper>
struct PV {
    using TMove = decltype(*std::declval<TIter>());

    TIter begin;
    TIter end;
    TMapper mapper;

    PV(TIter begin,
       TIter end,
       TMapper mapper = [](const TMove& move) { return move; });
};

/** upperbound */
struct Upperbound {};

/** displays */
struct Lowerbound {};

/**
 * Returns the info only if the specified condition
 * is met.
 *
 * Example:
 * report_info(Conditional(multipv_enabled, MultiPv(multipv_index));
 * // Will display 'multipv x' only if multipv_enabled is true.
 */
template <typename TInfo>
struct Conditional {
    bool condition;
    TInfo info;
    Conditional(bool condition,
                TInfo&& info);
};

} // info

class CommandContext {
public:
    /**
     * Returns an ArgReader that allows parsing the command's
     * arguments.
     */
    [[nodiscard]] ArgReader arg_reader() const;

    CommandContext() = delete;

private:
    std::string_view m_args;

    explicit CommandContext(std::string_view args);
    friend void main_loop();
};

class ArgReader {
public:
    /**
     * Rewinds to the start of the args string.
     */
    void rewind();

    /**
     * True if the command has been fully read.
     */
    [[nodiscard]] bool finished() const;

    /**
     * Skips whitespace and parses an integer.
     * If an integer cannot be parsed,
     * a InputError is thrown.
     */
    std::int64_t read_int();

    /**
     * Skips whitespace and parses an integer.
     * If a floating point number cannot be parsed,
     * a InputError is thrown.
     */
    double read_float();

    /**
     * Skips whitespace and parses an integer.
     * If an integer cannot be parsed,
     * std::nullopt is returned and the reader
     * remains in the state it was before
     * this method.
     */
    std::optional<std::int64_t> try_read_int();

    /**
     * Skips whitespace and parses a floating point number.
     * If a floating point number cannot be parsed,
     * std::nullopt is returned and the reader
     * remains in the state it was before
     * this method.
     */
    std::optional<double> try_read_float();

    /**
     * Skips whitespace and parses the next full sequence of characters
     * not composed by whitespace.
     */
    [[nodiscard]] std::string_view read_word();

    /**
     * Returns the unread portion of the command arg_reader.
     */
    [[nodiscard]] std::string_view peek_remainder() const;

    /**
     * Reads the command while the predicate is matched or the
     * command string ends.
     */
    std::string_view read_while(const std::function<bool(char)>& pred);

    /**
     * Reads the command until the predicate is matched or the
     * command string ends.
     */
    std::string_view read_until(const std::function<bool(char)>& pred);

    /**
     * Shorthand for read_while([](char c) { return std::isspace(c); }
     */
    void skip_whitespace();

    explicit ArgReader(std::string_view str);

private:
    std::string_view m_arg_str;
    size_t m_pos = 0;
};

//
// Internal implementation details
//

template <typename... TArgs>
void report_info(const TArgs&... args) {
    std::cout << "info";
    ((std::cout << ' ' << args), ...) << std::endl;
}

namespace info {

std::ostream& operator<<(std::ostream& stream, const String& info);
std::ostream& operator<<(std::ostream& stream, const Depth& info);
std::ostream& operator<<(std::ostream& stream, const SelDepth& info);
std::ostream& operator<<(std::ostream& stream, const HashFull& info);
std::ostream& operator<<(std::ostream& stream, const TbHits& info);
std::ostream& operator<<(std::ostream& stream, const Multipv& info);
std::ostream& operator<<(std::ostream& stream, const Upperbound& info);
std::ostream& operator<<(std::ostream& stream, const Lowerbound& info);
std::ostream& operator<<(std::ostream& stream, const Nodes& info);
std::ostream& operator<<(std::ostream& stream, const Nps& info);
std::ostream& operator<<(std::ostream& stream, const Time& info);
std::ostream& operator<<(std::ostream& stream, const CurrMoveNumber& info);
std::ostream& operator<<(std::ostream& stream, const Score& s);

template<typename TIter, typename TMapper>
PV<TIter, TMapper>::PV(TIter begin, TIter end, TMapper mapper)
    : begin(begin), end(end), mapper(mapper) { }

template <typename TInfo>
Conditional<TInfo>::Conditional(bool condition, TInfo&& info)
    : condition(condition), info(info) { }

template <typename TMove>
CurrMove<TMove>::CurrMove(TMove move)
    : move(move) { }

template<typename TIter, typename TMapper>
std::ostream& operator<<(std::ostream& stream, const PV<TIter, TMapper>& info) {
    stream << "pv";
    for (auto it = info.begin; it != info.end; ++it) {
        stream << ' ' << info.mapper(*it);
    }
    return stream;
}

template<typename T>
std::ostream& operator<<(std::ostream& stream, const Conditional<T> info) {
    if (info.condition) {
        return stream << info.info;
    }
    return stream;
}

template <typename TMove>
std::ostream& operator<<(std::ostream& stream, const CurrMove<TMove>& info) {
    return stream << "currmove " << info.move;
}

}

} // uci

#endif // UCI_H
