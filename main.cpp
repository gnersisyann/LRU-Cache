#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

template <typename K, typename V>
class CachePolicy;

template <typename K, typename V>
class ObjectCache
{
   public:
    struct CacheEntry
    {
        std::weak_ptr<V> value;
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point last_accessed_at;

        CacheEntry() = default;
        CacheEntry(const std::shared_ptr<V> v) : value(v), created_at(std::chrono::steady_clock::now()) { Access(); }

        void Access() { last_accessed_at = std::chrono::steady_clock::now(); }
    };

   public:
    explicit ObjectCache(size_t max_size = 1000) : max_size_(max_size) {}

    void SetPolicy(std::unique_ptr<CachePolicy<K, V>> new_policy)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        policy_ = std::move(new_policy);
    }

    void Put(const K& key, std::shared_ptr<V> val)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        CleanupExpired();

        if (policy_)
        {
            policy_->Evict(cache_);
        }
        else
        {
            cache_.erase(cache_.begin());
        }

        cache_[key] = CacheEntry(val);
    }

    std::optional<std::shared_ptr<V>> Get(const K& key)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_.find(key);
        if (it == cache_.end())
        {
            return std::nullopt;
        }

        if (it->second.value.expired())
        {
            cache_.erase(it);
            return std::nullopt;
        }
        it->second.Access();
        return it->second.value.lock();
    }

    void CleanupExpired()
    {
        for (auto it = cache_.begin(); it != cache_.end();)
        {
            if (it->second.value.expired())
            {
                it = cache_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    size_t Size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }

    void Clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
    }

    bool Contains(const K& key) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(key);
        if (cache_.find(key) != cache_.end() && !it->second.value.expired())
        {
            return true;
        }
        return false;
    }

   private:
    std::unordered_map<K, CacheEntry> cache_;
    std::unique_ptr<CachePolicy<K, V>> policy_;
    mutable std::mutex mutex_;
    size_t max_size_;
};

template <typename K, typename V>
class CachePolicy
{
   public:
    virtual ~CachePolicy() = default;
    virtual void Evict(std::unordered_map<K, typename ObjectCache<K, V>::CacheEntry>& cache) = 0;
};

template <typename K, typename V>
class LRUPolicy : public CachePolicy<K, V>
{
   public:
    void Evict(std::unordered_map<K, typename ObjectCache<K, V>::CacheEntry>& cache) override
    {
        auto lru = cache.begin();
        for (auto it = cache.begin(); it != cache.end(); ++it)
        {
            if (it->second.last_accessed_at < lru->second.last_accessed_at)
            {
                lru = it;
            }
        }
        if (lru != cache.end())
        {
            cache.erase(lru);
        }
    }
};

class Resource
{
   public:
    explicit Resource(std::string data) : data_(std::move(data))
    {
        std::cout << "Resource created: " << data_ << std::endl;
    }

    ~Resource() { std::cout << "Resource destroyed: " << data_ << std::endl; }

    std::string GetData() const { return data_; }

   private:
    std::string data_;
};

void TestObjectsDeletedAfterScope()
{
    ObjectCache<std::string, Resource> cache(2);
    cache.SetPolicy(std::make_unique<LRUPolicy<std::string, Resource>>());
    auto res1 = std::make_shared<Resource>("Resource 1");
    cache.Put("key1", res1);
    {
        auto temp = std::make_shared<Resource>("Temp resource");
        cache.Put("temp", temp);
    }
    std::cout << "Cache size: " << cache.Size() << std::endl;
    std::cout << "Contains key1: " << cache.Contains("key1") << std::endl;
    std::cout << "Contains temp: " << cache.Contains("temp") << std::endl;
    std::cout << "Value @temp exists: " << cache.Get("temp").has_value() << std::endl;
}

void TestLRU()
{
    ObjectCache<std::string, Resource> cache(2);
    cache.SetPolicy(std::make_unique<LRUPolicy<std::string, Resource>>());
    auto res1 = std::make_shared<Resource>("Resource 1");
    auto res2 = std::make_shared<Resource>("Resource 2");
    auto res3 = std::make_shared<Resource>("Resource 3");
    cache.Put("key1", res1);
    cache.Put("key2", res2);
    cache.Get("key1");
    cache.Put("key3", res2);

    std::cout << "Cache size: " << cache.Size() << std::endl;
    std::cout << "Contains key1: " << cache.Contains("key1") << std::endl;
    std::cout << "Contains key2: " << cache.Contains("key2") << std::endl;
    std::cout << "Contains key3: " << cache.Contains("key3") << std::endl;
}

int main()
{
    std::cout << ">>>>> OBJECTS DELETED AFTER SCOPE\n";
    TestObjectsDeletedAfterScope();
    std::cout << "\n>>>>> LRU\n";
    TestLRU();
}