#include "hnswlib/hnswlib.h"
#include "hnswlib.h"
#include <map>
#include <chrono>

namespace humanus {

void HNSWLibVectorStore::reset() {
    if (hnsw) {
        hnsw.reset();
    }
    if (space) {
        space.reset();
    }

    cache_map.clear();
    metadata_list.clear();
    
    if (config_->metric == VectorStoreConfig::Metric::L2) {
        space = std::make_shared<hnswlib::L2Space>(config_->dim);
        hnsw = std::make_shared<hnswlib::HierarchicalNSW<float>>(space.get(), config_->max_elements, config_->M, config_->ef_construction);
    } else if (config_->metric == VectorStoreConfig::Metric::IP) {
        space = std::make_shared<hnswlib::InnerProductSpace>(config_->dim);
        hnsw = std::make_shared<hnswlib::HierarchicalNSW<float>>(space.get(), config_->max_elements, config_->M, config_->ef_construction);
    } else {
        throw std::invalid_argument("Unsupported metric: " + std::to_string(static_cast<size_t>(config_->metric)));
    }
}

void HNSWLibVectorStore::insert(const std::vector<float>& vector, const size_t vector_id, const MemoryItem& metadata) {
    if (cache_map.size() >= config_->max_elements) {
        remove(metadata_list.back().id);
    }

    hnsw->addPoint(vector.data(), vector_id);
    
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    MemoryItem _metadata = metadata;
    if (_metadata.created_at < 0) {
        _metadata.created_at = now;
    }
    if (_metadata.updated_at < 0) {
        _metadata.updated_at = now;
    }
    
    set(vector_id, _metadata);
}

std::vector<MemoryItem> HNSWLibVectorStore::search(const std::vector<float>& query, size_t limit, const FilterFunc& filter) {
    auto filte_wrapper = filter ? std::make_unique<HNSWLibFilterFunctorWrapper>(*this, filter) : nullptr;
    auto results = hnsw->searchKnn(query.data(), limit, filte_wrapper.get());
    std::vector<MemoryItem> memory_items;
    
    while (!results.empty()) {
        const auto& [distance, id] = results.top();

        results.pop();
        
        if (cache_map.find(id) != cache_map.end()) {
            MemoryItem item = *cache_map[id]; 
            item.score = distance;
            memory_items.push_back(item);
        }
    }
    
    return memory_items;
}

void HNSWLibVectorStore::remove(size_t vector_id) {
    hnsw->markDelete(vector_id);
    auto it = cache_map.find(vector_id);
    if (it != cache_map.end()) {
        metadata_list.erase(it->second);
        cache_map.erase(it);
    }
}

void HNSWLibVectorStore::update(size_t vector_id, const std::vector<float>& vector, const MemoryItem& metadata) {
    if (!vector.empty()) {
        hnsw->markDelete(vector_id);
        hnsw->addPoint(vector.data(), vector_id);
    }

    if (!metadata.empty()) {
        MemoryItem new_metadata = metadata;
        new_metadata.id = vector_id; // Make sure the id is the same as the vector id
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        if (cache_map.find(vector_id) != cache_map.end()) {
            MemoryItem old_metadata = *cache_map[vector_id];
            if (new_metadata.hash == old_metadata.hash) {
                new_metadata.created_at = old_metadata.created_at;
            } else {
                new_metadata.created_at = now;
            }
        }
        if (new_metadata.created_at < 0) {
            new_metadata.created_at = now;
        }
        new_metadata.updated_at = now;
        set(vector_id, new_metadata);
    }
}

MemoryItem HNSWLibVectorStore::get(size_t vector_id) {
    auto it = cache_map.find(vector_id);
    if (it != cache_map.end()) {
        metadata_list.splice(metadata_list.begin(), metadata_list, it->second);
        return *it->second;
    }
    throw std::out_of_range("Vector id " + std::to_string(vector_id) + " not found in cache");
}

void HNSWLibVectorStore::set(size_t vector_id, const MemoryItem& metadata) {
    auto it = cache_map.find(vector_id);
    if (it != cache_map.end()) { // update existing metadata
        *it->second = metadata;
        metadata_list.splice(metadata_list.begin(), metadata_list, it->second);
    } else { // insert new metadata
        if (cache_map.size() >= config_->max_elements) { // cache full
            auto last = metadata_list.back();
            cache_map.erase(last.id);
            metadata_list.pop_back();
        }

        metadata_list.emplace_front(metadata);
        cache_map[vector_id] = metadata_list.begin();
    }
}

std::vector<MemoryItem> HNSWLibVectorStore::list(size_t limit, const FilterFunc& filter) {
    std::vector<MemoryItem> result;
    size_t count = hnsw->cur_element_count;
    
    for (size_t i = 0; i < count; i++) {
        if (!hnsw->isMarkedDeleted(i)) {
            auto memory_item = get(i);
            if (filter && !filter(memory_item)) {
                continue;
            }
            result.emplace_back(memory_item);
            if (limit > 0 && result.size() >= limit) {
                break;
            }
        }
    }
    
    return result;
}

};
