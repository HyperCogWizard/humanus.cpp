#include "base.h"
#include "hnswlib.h"

namespace humanus {

std::unordered_map<std::string, std::shared_ptr<VectorStore>> VectorStore::instances_;

std::shared_ptr<VectorStore> VectorStore::get_instance(const std::string& config_name, const std::shared_ptr<VectorStoreConfig>& config) {
    if (instances_.find(config_name) == instances_.end()) {
        auto config_ = config;
        if (!config_) {
            config_ = std::make_shared<VectorStoreConfig>(Config::get_vector_store_config(config_name));
        }

        if (config_->provider == "hnswlib") {
            instances_[config_name] = std::make_shared<HNSWLibVectorStore>(config_);
        } else {
            throw std::invalid_argument("Unsupported embedding model provider: " + config_->provider);
        }
    }
    return instances_[config_name];
}

} // namespace humanus