#ifndef HUMANUSlogger_H
#define HUMANUSlogger_H

#include "config.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/dist_sink.h"
#include <string>
#include <filesystem>
#include <mutex>
#include <unordered_set>

namespace humanus {

static spdlog::level::level_enum _print_level = spdlog::level::info;
static spdlog::level::level_enum _logfile_level = spdlog::level::debug;

extern std::shared_ptr<spdlog::logger> set_log_level(spdlog::level::level_enum print_level, spdlog::level::level_enum logfile_level);

class SessionSink : public spdlog::sinks::base_sink<std::mutex> {
private:
    inline static std::unordered_map<std::string, std::vector<std::string>> buffers_;    // session_id -> buffer
    inline static std::unordered_map<std::string, std::vector<std::string>> histories_;  // session_id -> history
    inline static std::unordered_map<std::string, std::string> sessions_;                // thread_id  -> session_id
    inline static std::mutex mutex_;

    SessionSink() = default;
    SessionSink(const SessionSink&) = delete;
    SessionSink& operator=(const SessionSink&) = delete;

public:
    ~SessionSink() = default;

    static std::shared_ptr<SessionSink> get_instance() {
        static SessionSink instance;
        static std::shared_ptr<SessionSink> shared_instance(&instance, [](SessionSink*){});
        return shared_instance;
    }

    void sink_it_(const spdlog::details::log_msg& msg) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (sessions_.find(get_thread_id()) == sessions_.end()) { // Ignore messages if session_id is not set
            return;
        }

        auto session_id = sessions_[get_thread_id()];

        auto time_t = std::chrono::system_clock::to_time_t(msg.time);
        auto tm = fmt::localtime(time_t);
        std::string log_message = fmt::format("[{:%Y-%m-%d %H:%M:%S}] {}", tm, msg.payload);
         
        buffers_[session_id].push_back(log_message);
    }

    void flush_() override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (sessions_.find(get_thread_id()) == sessions_.end()) { // Ignore messages if session_id is not set
            return;
        }

        auto session_id = sessions_[get_thread_id()];

        if (!buffers_[session_id].empty()) {
            histories_[session_id].insert(histories_[session_id].end(), 
                                          buffers_[session_id].begin(), 
                                          buffers_[session_id].end());
            buffers_[session_id].clear();
        }
    }

    inline std::string get_thread_id() {
        std::stringstream ss;
        ss << std::this_thread::get_id();
        return ss.str();
    }

    void set_session_id(std::string session_id) {
        if (session_id.empty()) {
            throw std::invalid_argument("session_id is empty");
        }
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_[get_thread_id()] = session_id;
    }

    // Messages in buffer are flushed to the history and cleared from the buffer.
    std::vector<std::string> get_buffer(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (buffers_.find(session_id) == buffers_.end()) {
            throw std::invalid_argument("Invalid session_id: " + session_id);
        }
        
        std::vector<std::string> buffer = buffers_[session_id];
        if (!buffers_[session_id].empty()) {
            histories_[session_id].insert(histories_[session_id].end(), 
                                        buffers_[session_id].begin(), 
                                        buffers_[session_id].end());
            buffers_[session_id].clear();
        }
        return buffer;
    }

    std::vector<std::string> get_history(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (histories_.find(session_id) == histories_.end()) {
            throw std::invalid_argument("Invalid session_id: " + session_id);
        }

        return histories_[session_id];
    }

    void clear_buffer() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (sessions_.find(get_thread_id()) == sessions_.end()) { // Ignore messages if session_id is not set
            return;
        }

        auto session_id = sessions_[get_thread_id()];

        if (!buffers_[session_id].empty()) {
            histories_[session_id].insert(histories_[session_id].end(), 
                                          buffers_[session_id].begin(), 
                                          buffers_[session_id].end());
            buffers_[session_id].clear();
        }
    }

    void clear_history() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (sessions_.find(get_thread_id()) == sessions_.end()) { // Ignore messages if session_id is not set
            return;
        }

        auto session_id = sessions_[get_thread_id()];
        
        histories_[session_id].clear();
    }

    void cleanup_session(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto it = sessions_.begin(); it != sessions_.end();) {
            if (it->second == session_id) {
                it = sessions_.erase(it);
            } else {
                ++it;
            }
        }

        buffers_.erase(session_id);
        histories_.erase(session_id);
    }

    std::vector<std::string> get_active_sessions() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> result;
        std::unordered_set<std::string> unique_sessions;
        
        for (const auto& [thread_id, session_id] : sessions_) {
            if (unique_sessions.insert(session_id).second) {
                result.push_back(session_id);
            }
        }
        
        return result;
    }
};

static std::shared_ptr<spdlog::logger> logger = set_log_level(_print_level, _logfile_level);

} // namespace humanus

#endif