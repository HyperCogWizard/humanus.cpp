#include "base.h"
#include "oai.h"

namespace humanus {

std::unordered_map<std::string, std::shared_ptr<EmbeddingModel>> EmbeddingModel::instances_;

std::shared_ptr<EmbeddingModel> EmbeddingModel::get_instance(const std::string& config_name, const std::shared_ptr<EmbeddingModelConfig>& config)  {
    if (instances_.find(config_name) == instances_.end()) {
        auto config_ = config;
        if (!config_) {
            config_ = std::make_shared<EmbeddingModelConfig>(Config::get_embedding_model_config(config_name));
        }

        if (config_->provider == "oai") {
            instances_[config_name] = std::make_shared<OAIEmbeddingModel>(config_);
        } else {
            throw std::invalid_argument("Unsupported embedding model provider: " + config_->provider);
        }
    }
    return instances_[config_name];
}

} // namespace humanus