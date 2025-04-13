#include "logger.h"
#include "config.h"
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace humanus {

std::shared_ptr<spdlog::logger> set_log_level(spdlog::level::level_enum print_level, spdlog::level::level_enum logfile_level) {
    auto current_date = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(current_date);
    
    std::stringstream ss;
    std::tm tm_info = *std::localtime(&in_time_t);
    ss << std::put_time(&tm_info, "%Y-%m-%d");
    std::string formatted_date = ss.str(); // YYYY-MM-DD
    
    std::string log_name = formatted_date;
    std::string log_file_path = (PROJECT_ROOT / "logs" / (log_name + ".log")).string();

    // Ensure the log directory exists
    std::filesystem::create_directories(PROJECT_ROOT / "logs");

    // Reset the log output
    std::shared_ptr<spdlog::logger> _logger = std::make_shared<spdlog::logger>(log_name);
    
    auto stderr_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    stderr_sink->set_level(_print_level);
    _logger->sinks().push_back(stderr_sink);
    
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_path, false);
    file_sink->set_level(_logfile_level);
    _logger->sinks().push_back(file_sink);

    auto session_sink = SessionSink::get_instance();
    session_sink->set_level(_print_level);
    _logger->sinks().push_back(session_sink);

    // Reset the log output
    return _logger;
}

} // namespace humanus 