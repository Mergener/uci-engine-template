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

#include "uci.h"

#include <iostream>
#include <map>
#include <unordered_map>
#include <string>
#include <utility>
#include <variant>
#include <memory>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <chrono>

namespace uci {

//
// Forward declarations
//

std::ostream& operator<<(std::ostream& stream, const OptionType& type);

//
// UCI Options
//

// The values in this variant *must* match the same
// order of the OptionType enum.
using OptionValue = std::variant<
    std::monostate,
    std::int64_t,
    std::string,
    bool>;

struct Option {
    OptionValue current;
    std::function<void(const OptionValue&)> change_handler;
    OptionValue default_value;
    std::int64_t min {};
    std::int64_t max {};

    [[nodiscard]] OptionType type() const;
};

OptionType Option::type() const {
    return static_cast<OptionType>(current.index());
}

static std::map<std::string, Option> s_options;

template <typename T>
const T& get_option(const std::string& name) {
    try {
        return std::get<T>(s_options.at(name).current);
    }
    catch (const std::bad_variant_access& e) {
        throw InputError("Invalid option type.");
    }
    catch (const std::out_of_range& e) {
        throw InputError("Option " + name + " not found.");
    }
}

const std::string& get_string_option(const std::string& name) {
    return get_option<std::string>(name);
}

std::int64_t get_spin_option(const std::string& name) {
    return get_option<std::int64_t>(name);
}

bool get_check_option(const std::string& name) {
    return get_option<bool>(name);
}

void set_option(const std::string& name, const OptionValue& value) {
    Option& opt = s_options.at(name);
    if (opt.current.index() != value.index()) {
        throw std::invalid_argument("Value type doesn't match the option's type.");
    }

    if (static_cast<OptionType>(opt.current.index()) == OptionType::Spin) {
        std::int64_t value_as_int = std::get<std::int64_t>(value);
        if (value_as_int > opt.max) {
            throw InputError("Maximum value for option " + name + " is " + std::to_string(opt.max) + ".");
        }
        if (value_as_int < opt.min) {
            throw InputError("Minimum value for option " + name + " is " + std::to_string(opt.min) + ".");
        }
    }
    opt.current = value;
    opt.change_handler(value);
}

void register_button_option(const std::string& name,
                            const std::function<void()>& trigger_handler) {
    s_options[name] = Option {
        std::monostate(),
        [=](const OptionValue& v) { trigger_handler(); },
    };
}

void register_check_option(const std::string& name,
                           bool default_value,
                           const std::function<void(bool)>& change_handler) {
    s_options[name] = Option {
        default_value,
        [=](const OptionValue& v) { change_handler(std::get<std::int64_t>(v)); },
        default_value,
    };
}

void register_spin_option(const std::string& name,
                          std::int64_t default_value,
                          std::int64_t min,
                          std::int64_t max,
                          const std::function<void(std::int64_t)>& change_handler) {
    s_options[name] = Option {
        default_value,
        [=](const OptionValue& v) { change_handler(std::get<std::int64_t>(v)); },
        default_value,
        min,
        max
    };
}

void register_string_option(const std::string& name,
                            std::string default_value,
                            const std::function<void(const std::string&)>& change_handler) {
    s_options[name] = Option {
        default_value,
        [=](const OptionValue& v) { change_handler(std::get<std::string>(v)); },
        default_value
    };
}

std::vector<OptionInfo> get_all_options() {
    std::vector<OptionInfo> vec;
    for (const auto& pair: s_options) {
        vec.push_back(OptionInfo {
            pair.second.current,
            pair.first,
            pair.second.type(),
            pair.second.default_value,
            pair.second.min,
            pair.second.max
        });
    }
    return vec;
}

OptionInfo get_option_info(const std::string& opt_name) {
    auto& option = s_options.at(opt_name);
    return OptionInfo {
        option.current,
        opt_name,
        option.type(),
        option.default_value,
        option.min,
        option.max
    };
}

std::ostream& operator<<(std::ostream& stream, const OptionType& type) {
    switch (type) {
        case OptionType::Spin:   return stream << "spin";
        case OptionType::String: return stream << "string";
        case OptionType::Check:  return stream << "check";
        default:                 return stream << "unknown";
    }
}


//
// Command Handlers
//

static void default_error_handler(const std::exception& e) {
    std::cerr << "Fatal:\n" << e.what() << std::endl;
    std::terminate();
}

static std::unordered_map<std::string, std::function<void(CommandContext&)>> s_cmd_handlers {};
static std::function<void(const std::exception& e)> s_err_handler = default_error_handler;

void set_error_handler(std::function<void(const std::exception& e)> handler) {
    s_err_handler = std::move(handler);
}

void register_custom_command(std::string command,
                             std::function<void(CommandContext& ctx)> handler) {
    s_cmd_handlers[command] = std::move(handler);
}

void register_quit() {
    register_custom_command("quit", [=](const CommandContext& ctx) {
        std::exit(0);
    });
}

void register_isready() {
    register_custom_command("isready", [=](const CommandContext& ctx) {
        std::cout << "readyok" << std::endl;
    });
}

void register_ucinewgame(const std::function<void()>&fn) {
    register_custom_command("ucinewgame", [=](const CommandContext& ctx) {
        fn();
    });
}

void register_stop(const std::function<void()>&fn) {
    register_custom_command("stop", [=](const CommandContext& ctx) {
        fn();
    });
}

void register_go(const std::function<void(const GoArgs&)>& handler) {
    register_custom_command("go", [=](const CommandContext& ctx) {
        ArgReader reader = ctx.arg_reader();
        GoArgs go_args {};

        std::string_view word = reader.read_word();
        while (!word.empty()) {
            if (word == "infinite") {
                if (!go_args.infinite) {
                    throw InputError("Unexpected infinite to go when limits were specified.");
                }
                continue;
            }
            go_args.infinite = false;

            if (word == "wtime") {
                go_args.w_time = reader.read_int();
            }
            else if (word == "winc") {
                go_args.w_inc = reader.read_int();
            }
            else if (word == "btime") {
                go_args.b_time = reader.read_int();
            }
            else if (word == "binc") {
                go_args.b_inc = reader.read_int();
            }
            else if (word == "nodes") {
                go_args.nodes = reader.read_int();
            }
            else if (word == "depth") {
                go_args.depth = reader.read_int();
            }
            else if (word == "movetime") {
                go_args.move_time = reader.read_int();
            }
            else {
                throw InputError("Unexpected argument for go: " + std::string(word));
            }
            word = reader.read_word();
        }

        handler(go_args);
    });
}

void register_position(const std::function<void(const PositionArgs&)>& handler) {
    register_custom_command("position", [=](const CommandContext& ctx) {
        ArgReader reader = ctx.arg_reader();
        PositionArgs args {};

        // Check first argument.
        std::string_view word = reader.read_word();
        if (word == "startpos") {
            args.fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        }
        else if (word == "fen") {
            std::stringstream fen_stream;
            while (!reader.finished()) {
                reader.skip_whitespace();
                std::string_view remainder = reader.peek_remainder();
                if (   remainder.size() >= 5
                    && remainder.substr(0, 5) == "moves") {
                    break;
                }
                fen_stream << reader.read_word() << ' ';
            }
            args.fen = fen_stream.str();
        }
        else if (word.empty()) {
            throw InputError("Expected a position specifier (fen or startpos)");
        }
        else {
            throw InputError("Unexpected argument to position: " + std::string(word));
        }

        // Check if the user requested moves.
        word = reader.read_word();
        if (!word.empty()) {
            if (word != "moves") {
                throw InputError("Unexpected argument to position: " + std::string(word));
            }

            // User has requested moves. Add them.
            word = reader.read_word();
            while (!word.empty()) {
                args.moves.emplace_back(word);
                word = reader.read_word();
            }
        }

        handler(args);
    });
}

void register_uci(const std::string& engine_name,
                  const std::string& author_name) {
    register_custom_command("uci", [=](const CommandContext& ctx) {
        if (!engine_name.empty()) {
            std::cout << "id name " << engine_name << '\n';
        }
        else {
            std::cout << "id name Unnamed Engine\n";
        }
        if (!author_name.empty()) {
            std::cout << "id author " << author_name << '\n';
        }

       std::vector<OptionInfo> options = get_all_options();
       for (const OptionInfo& opt: options) {
           std::cout << "option " << opt.name << " type " << opt.type;

           switch (opt.type) {
               case OptionType::Spin:
                   std::cout << " default "
                             << std::get<std::int64_t>(opt.default_value)
                             << " min "
                             << std::get<std::int64_t>(opt.min)
                             << " max "
                             << std::get<std::int64_t>(opt.max);
                   break;

               case OptionType::String:
                   std::cout << " default "
                             << std::get<std::string>(opt.default_value);
                   break;

               case OptionType::Check:
                   std::cout << " default "
                             << (std::get<bool>(opt.default_value) ? "true" : "false");
                   break;

               default:
                   break;
           }

           std::cout << '\n';
       }

       std::cout << "uciok" << std::endl;
    });
}

void register_setoption() {
    register_custom_command("setoption", [](const CommandContext& ctx){
        ArgReader reader = ctx.arg_reader();

        if (reader.read_word() != "name") {
            throw InputError("Expected 'name'.");
        }

        std::string name = std::string(reader.read_word());
        if (name.empty()) {
            throw InputError("Expected an option name.");
        }

        // Check if the option exists.
        auto it = s_options.find(name);
        if (it == s_options.end()) {
            throw InputError("No such option: " + name);
        }
        const Option& option = it->second;

        if (reader.read_word() != "value") {
            throw InputError("Expected 'value'.");
        }

        std::string value = std::string(reader.read_word());

        switch (option.type()) {
            case OptionType::Button:
                set_option(name, std::monostate());
                break;
            case OptionType::Spin:
                set_option(name, std::stoll(value));
                break;
            case OptionType::String:
                set_option(name, value);
                break;
            case OptionType::Check:
                set_option(name, value == "true");
                break;
            default:
                throw InputError("Unexpected option");
        }
    });
}

//
// Reporting
//

void report_best_move(const std::string& move_str,
                      const std::string& ponder_move_str) {
    std::cout << "bestmove " << move_str;
    if (!ponder_move_str.empty() && ponder_move_str != "0000") {
        std::cout << " ponder " << ponder_move_str;
    }
    std::cout << std::endl;
}

namespace info {

String::String(std::string str) : str(std::move(str)) {}
std::ostream& operator<<(std::ostream& stream, const String& info) {
    return stream << "string " << info.str;
}

Depth::Depth(int depth) : depth(depth) {}
std::ostream& operator<<(std::ostream& stream, const Depth& info) {
    return stream <<  "depth " << info.depth;
}

SelDepth::SelDepth(int sel_depth) : sel_depth(sel_depth) {}
std::ostream& operator<<(std::ostream& stream, const SelDepth& info) {
    return stream <<  "seldepth " << info.sel_depth;
}

HashFull::HashFull(int hash_full) : hash_full(hash_full) {}
std::ostream& operator<<(std::ostream& stream, const HashFull& info) {
    return stream <<  "hashfull " << info.hash_full;
}

TbHits::TbHits(std::uint64_t tb_hits) : tb_hits(tb_hits) {}
std::ostream& operator<<(std::ostream& stream, const TbHits& info) {
    return stream <<  "tbhits " << info.tb_hits;
}

Multipv::Multipv(int multipv) : multipv(multipv) {}
std::ostream& operator<<(std::ostream& stream, const Multipv& info) {
    return stream << "multipv " << info.multipv;
}

Nodes::Nodes(std::uint64_t nodes) : nodes(nodes) {}
std::ostream& operator<<(std::ostream& stream, const Nodes& info) {
    return stream << "nodes " << info.nodes;
}

Nps::Nps(std::uint64_t nps) : nps(nps) {}
std::ostream& operator<<(std::ostream& stream, const Nps& info) {
    return stream << "nps " << info.nps;
}

Time::Time(std::uint64_t time) : time(time) {}
std::ostream& operator<<(std::ostream& stream, const Time& info) {
    return stream <<  "time " << info.time;
}

CurrMoveNumber::CurrMoveNumber(int num) : curr_move_number(num) {}
std::ostream& operator<<(std::ostream& stream, const CurrMoveNumber& info) {
    return stream <<  "currmovenumber " << info.curr_move_number;
}

std::ostream& operator<<(std::ostream& stream, const Upperbound&) {
    return stream << "upperbound";
}

std::ostream& operator<<(std::ostream& stream, const Lowerbound&) {
    return stream << "lowerbound";
}

Score::Score(int score) : score(score), allow_mate_scores(false) {}
Score::Score(int score,
             int mate,
             int max_mate_plies) : score(score),
                                   allow_mate_scores(true),
                                   mate_score(std::abs(mate)),
                                   mate_threshold(mate - max_mate_plies) {}
std::ostream& operator<<(std::ostream& stream, const Score& s) {
    if (!s.allow_mate_scores || std::abs(s.score) < s.mate_threshold) {
        return stream << "score cp " << s.score;
    }
    int plies = s.mate_score - std::abs(s.score);
    int moves = (plies + 1) / 2;
    return stream << "score mate " << (s.score > 0 ? moves : -moves);
}

} // info

//
// Work thread
//

class WorkThread {
public:
    using Task = std::function<void(StopSignal)>;

    void submit_task(Task new_task);
    void stop_current_task();
    bool running() const;

    WorkThread();
    ~WorkThread();

private:
    std::thread m_thread;
    Task m_task = nullptr;
    std::mutex m_mutex;
    std::condition_variable m_cond_var;
    std::atomic<bool> m_kill = false;
    std::atomic<bool> m_stop = false;

    bool should_stop();
    void run();
};

bool WorkThread::running() const {
    return m_task != nullptr;
}

bool WorkThread::should_stop() {
    return m_stop.load(std::memory_order_relaxed);
}

void WorkThread::run() {
    while (true) {
        Task task_to_run = nullptr;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cond_var.wait(lock, [this] { return m_task || m_kill; });

            if (m_kill) {
                return;
            }
            task_to_run = m_task;
            m_task = nullptr;
        }
        if (task_to_run) {
            task_to_run([this]() { return should_stop(); });
        }
    }
}

void WorkThread::submit_task(Task new_task) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_task = std::move(new_task);
        m_stop.store(false, std::memory_order_relaxed);
    }
    m_cond_var.notify_one();
}

void WorkThread::stop_current_task() {
    m_stop.store(true, std::memory_order_acquire);
}

WorkThread::WorkThread()
    : m_thread(std::thread(&WorkThread::run, this)) { }

WorkThread::~WorkThread() {
    stop_current_task();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_kill = true;
    }
    m_cond_var.notify_one();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

static std::unique_ptr<WorkThread> s_work_thread = nullptr;

void awake_work_thread() {
    if (s_work_thread) {
        return;
    }

    s_work_thread = std::make_unique<WorkThread>();
}

void launch_work_thread(const std::function<void(StopSignal)>& task) {
    awake_work_thread();
    stop_work_thread();
    s_work_thread->submit_task(task);
}

void stop_work_thread() {
    if (!s_work_thread) {
        return;
    }

    s_work_thread->stop_current_task();
}

bool work_thread_running() {
    if (!s_work_thread) {
        return false;
    }

    return s_work_thread->running();
}

//
// Argument parsing
//

InputError::InputError(const std::string& s)
    : std::runtime_error(s) { }

CommandContext::CommandContext(std::string_view args)
    : m_args(args){ }

ArgReader CommandContext::arg_reader() const {
    return ArgReader(m_args);
}

ArgReader::ArgReader(std::string_view str)
    : m_arg_str(str), m_pos(0) {}

void ArgReader::rewind() {
    m_pos = 0;
}

bool ArgReader::finished() const {
    return m_pos >= m_arg_str.size();
}

std::int64_t ArgReader::read_int() {
    auto n = try_read_int();

    if (!n.has_value()) {
        throw InputError("Expected an integer number.");
    }

    return *n;
}

std::optional<std::int64_t> ArgReader::try_read_int() {
    size_t pos_before = m_pos;
    skip_whitespace();

    std::string_view int_str = read_word();
    try {
        return std::stoll(std::string(int_str));
    } catch (const std::invalid_argument&) {
        m_pos = pos_before;
        return std::nullopt;
    }
}

double ArgReader::read_float() {
    auto n = try_read_float();

    if (!n.has_value()) {
        throw InputError("Expected a float number.");
    }

    return *n;
}

std::optional<double> ArgReader::try_read_float() {
    size_t pos_before = m_pos;
    skip_whitespace();

    std::string_view float_str = read_word();
    try {
        return std::stod(std::string(float_str));
    } catch (const std::invalid_argument&) {
        m_pos = pos_before;
        return std::nullopt;
    }
}

std::string_view ArgReader::read_until(const std::function<bool(char)>& pred) {
    if (finished()) {
        return "";
    }
    size_t start_pos = m_pos;
    while (m_pos < m_arg_str.size() && !pred(m_arg_str[m_pos])) {
        ++m_pos;
    }
    return m_arg_str.substr(start_pos, m_pos - start_pos);
}

std::string_view ArgReader::read_while(const std::function<bool(char)>& pred) {
    if (finished()) {
        return "";
    }

    size_t start_pos = m_pos;
    while (m_pos < m_arg_str.size() && pred(m_arg_str[m_pos])) {
        ++m_pos;
    }
    return m_arg_str.substr(start_pos, m_pos - start_pos);
}

void ArgReader::skip_whitespace() {
    read_while([](char c) { return std::isspace(c);});
}

std::string_view ArgReader::peek_remainder() const {
    return m_arg_str.substr(m_pos);
}

std::string_view ArgReader::read_word() {
    skip_whitespace();
    return read_until([](char c) { return std::isspace(c); });
}

//
// Main loop
//

void main_loop() {
    std::string line;
    while (std::getline(std::cin, line)) {
        try {
            ArgReader reader(line);
            std::string_view command = reader.read_word();

            auto it = s_cmd_handlers.find(std::string(command));
            if (it == s_cmd_handlers.end()) {
                std::cerr << "Unknown command." << std::endl;
                continue;
            }

            try {
                reader.skip_whitespace();
                CommandContext ctx(reader.peek_remainder());
                it->second(ctx);
            }
            catch (const InputError& e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }
        }
        catch (const std::exception& e) {
            s_err_handler(e);
        }
    }
}

} // uci