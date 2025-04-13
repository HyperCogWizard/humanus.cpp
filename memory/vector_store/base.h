#ifndef HUMANUS_MEMORY_VECTOR_STORE_BASE_H
#define HUMANUS_MEMORY_VECTOR_STORE_BASE_H

#include "config.h"
#include <memory>
#include <unordered_map>
#include <string>

namespace humanus {

class VectorStore {
private:
    static std::unordered_map<std::string, std::shared_ptr<VectorStore>> instances_;

protected:
    std::shared_ptr<VectorStoreConfig> config_;

    // Constructor
    VectorStore(const std::shared_ptr<VectorStoreConfig>& config) : config_(config) {}

public:
    // Get the singleton instance
    static std::shared_ptr<VectorStore> get_instance(const std::string& config_name = "default", const std::shared_ptr<VectorStoreConfig>& config = nullptr);

    virtual ~VectorStore() = default;

    virtual void reset()  = 0;

    /**
     * @brief Insert a vector with metadata
     * @param vector vector data
     * @param vector_id vector ID
     * @param metadata metadata
     */
    virtual void insert(const std::vector<float>& vector, 
                        const size_t vector_id, 
                        const MemoryItem& metadata = MemoryItem()) = 0;

    /**
     * @brief Search similar vectors
     * @param query query vector
     * @param limit limit of returned results
     * @param filter optional filter (returns true for allowed vectors)
     * @return list of similar vectors
     */
    virtual std::vector<MemoryItem> search(const std::vector<float>& query, 
                                            size_t limit = 5, 
                                            const FilterFunc& filter = nullptr) = 0;

    /**
     * @brief Remove a vector by ID
     * @param vector_id vector ID
     */
    virtual void remove(size_t vector_id) = 0;

    /**
     * @brief Update a vector and its metadata
     * @param vector_id vector ID
     * @param vector new vector data
     * @param metadata new metadata
     */
    virtual void update(size_t vector_id, const std::vector<float>& vector = std::vector<float>(), const MemoryItem& metadata = MemoryItem()) = 0;

    /**
     * @brief Get a vector by ID
     * @param vector_id vector ID
     * @return vector data
     */
    virtual MemoryItem get(size_t vector_id) = 0;

    /**
     * @brief Set metadata for a vector
     * @param vector_id vector ID
     * @param metadata new metadata
     */
    virtual void set(size_t vector_id, const MemoryItem& metadata) = 0;

    /**
     * @brief List all memories
     * @param limit optional limit of returned results
     * @param filter optional filter (returns true for allowed memories)
     * @return list of memories
     */
    virtual std::vector<MemoryItem> list(size_t limit = 0, const FilterFunc& filter = nullptr) = 0;
};

} // namespace humanus


#endif // HUMANUS_MEMORY_VECTOR_STORE_BASE_H
