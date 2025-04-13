#include "oai.h"

namespace humanus {

std::vector<float> OAIEmbeddingModel::embed(const std::string& text, EmbeddingType /* type */) {
    json body = {
        {"model", config_->model},
        {"input", text},
        {"encoding_format", "float"}
    };
    
    std::string body_str = body.dump();

    int retry = 0;

    while (retry <= config_->max_retries) {
        // send request
        auto res = client_->Post(config_->endpoint, body_str, "application/json");

        if (!res) {
            logger->error(std::string(__func__) + ": Failed to send request: " + httplib::to_string(res.error()));
        } else if (res->status == 200) {
            try {
                json json_data = json::parse(res->body);
                return json_data["data"][0]["embedding"].get<std::vector<float>>();
            } catch (const std::exception& e) {
                logger->error(std::string(__func__) + ": Failed to parse response: error=" + std::string(e.what()) + ", body=" + res->body);
            }
        } else {
            logger->error(std::string(__func__) + ": Failed to send request: status=" + std::to_string(res->status) + ", body=" + res->body);
        }

        retry++;

        if (retry > config_->max_retries) {
            break;
        }

        // wait for a while before retrying
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        logger->info("Retrying " + std::to_string(retry) + "/" + std::to_string(config_->max_retries));
    }

    // If the logger has a file sink, log the request body
    if (logger->sinks().size() > 1) {
        auto file_sink = std::dynamic_pointer_cast<spdlog::sinks::basic_file_sink_mt>(logger->sinks()[1]);
        if (file_sink) {
            file_sink->log(spdlog::details::log_msg(
                spdlog::source_loc{},
                logger->name(),
                spdlog::level::debug,
                "Failed to get response from embedding model. Full request body: " + body_str
            ));
        }
    }

    throw std::runtime_error("Failed to get embedding from: " + config_->base_url + " " + config_->model);
}

} // namespace humanus