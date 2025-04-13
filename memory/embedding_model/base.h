#ifndef HUMANUS_MEMORY_EMBEDDING_MODEL_BASE_H
#define HUMANUS_MEMORY_EMBEDDING_MODEL_BASE_H

#include "httplib.h"
#include "logger.h"
#include <vector>
#include <unordered_map>
#include <memory>

namespace humanus {

class EmbeddingModel {
private:
    static std::unordered_map<std::string, std::shared_ptr<EmbeddingModel>> instances_;

protected:
    std::shared_ptr<EmbeddingModelConfig> config_;
    
    // Constructor
    EmbeddingModel(const std::shared_ptr<EmbeddingModelConfig>& config) : config_(config) {}

public:
    // Get the singleton instance
    static std::shared_ptr<EmbeddingModel> get_instance(const std::string& config_name = "default", const std::shared_ptr<EmbeddingModelConfig>& config = nullptr);

    virtual ~EmbeddingModel() = default;

    virtual std::vector<float> embed(const std::string& text, EmbeddingType type) = 0;
};

} // namespace humanus

#endif // HUMANUS_MEMORY_EMBEDDING_MODEL_BASE_H