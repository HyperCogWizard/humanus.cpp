#ifndef HUMANUS_MEMORY_EMBEDDING_MODEL_OAI_H
#define HUMANUS_MEMORY_EMBEDDING_MODEL_OAI_H

#include "base.h"

namespace humanus {

class OAIEmbeddingModel : public EmbeddingModel {
private:
    std::unique_ptr<httplib::Client> client_;

public:
    OAIEmbeddingModel(const std::shared_ptr<EmbeddingModelConfig>& config) : EmbeddingModel(config) {
        client_ = std::make_unique<httplib::Client>(config_->base_url);
        client_->set_default_headers({
            {"Authorization", "Bearer " + config_->api_key}
        });
    }

    std::vector<float> embed(const std::string& text, EmbeddingType type) override;
};

} // namespace humanus

#endif // HUMANUS_MEMORY_EMBEDDING_MODEL_OAI_H