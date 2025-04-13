#ifndef HUMANUS_MEMORY_VECTOR_STORE_HNSWLIB_H
#define HUMANUS_MEMORY_VECTOR_STORE_HNSWLIB_H

#include "base.h"
#include "hnswlib/hnswlib.h"
#include <list>

namespace humanus {

class HNSWLibVectorStore : public VectorStore {
private:
    std::shared_ptr<hnswlib::HierarchicalNSW<float>> hnsw;
    std::shared_ptr<hnswlib::SpaceInterface<float>> space;
    std::unordered_map<size_t, std::list<MemoryItem>::iterator> cache_map; // LRU cache
    std::list<MemoryItem> metadata_list; // Metadata for stored vectors

public:
    HNSWLibVectorStore(const std::shared_ptr<VectorStoreConfig>& config) : VectorStore(config) {
        reset();
    }

    void reset() override;

    void insert(const std::vector<float>& vector, const size_t vector_id, const MemoryItem& metadata = MemoryItem()) override;

    std::vector<MemoryItem> search(const std::vector<float>& query, size_t limit, const FilterFunc& filter = nullptr) override;

    void remove(size_t vector_id) override;

    void update(size_t vector_id, const std::vector<float>& vector = std::vector<float>(), const MemoryItem& metadata = MemoryItem()) override;

    MemoryItem get(size_t vector_id) override;

    void set(size_t vector_id, const MemoryItem& metadata) override;

    std::vector<MemoryItem> list(size_t limit, const FilterFunc& filter = nullptr) override;
};

class HNSWLibFilterFunctorWrapper : public hnswlib::BaseFilterFunctor {
private:
    HNSWLibVectorStore& vector_store;
    FilterFunc filter_func;

public:
    HNSWLibFilterFunctorWrapper(HNSWLibVectorStore& store, const FilterFunc& filter_func)
    : vector_store(store), filter_func(filter_func) {}

    bool operator()(hnswlib::labeltype id) override {
        if (filter_func == nullptr) {
            return true;
        }
        
        try {
            return filter_func(vector_store.get(id));
        } catch (...) {
            return false;
        }
    }
};

}

#endif // HUMANUS_MEMORY_VECTOR_STORE_HNSWLIB_H