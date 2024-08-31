#pragma once

// Std dependencies
#include <string>
#include <unordered_map>

// Singleton that acts as a blackboard for resources
// to be shared between different parts of the tasks
class ResourceManager
{
public:
	static ResourceManager& GetInstance()
	{
		static ResourceManager instance;
		return instance;
	}

	ResourceManager(ResourceManager const&) = delete;
	void operator=(ResourceManager const&) = delete;

	// Public methods
	template<typename TType>
	inline void SetResource(const std::string& key, TType* resource);
	inline void SetResource(const std::string& key, void* resource);

	template<typename TType>
	inline bool TryGetResource(const std::string& key, TType*& resource);
	inline bool TryGetResource(const std::string& key, void*& resource);

private:
	ResourceManager() = default;
	~ResourceManager() = default;

	std::unordered_map<std::string, void*> _resources;
};

template<typename TType>
inline void ResourceManager::SetResource(const std::string& key, TType* resource)
{
	_resources[key] = resource;
}

inline void ResourceManager::SetResource(const std::string& key, void* resource)
{
	_resources[key] = resource;
}

template<typename TType>
inline bool ResourceManager::TryGetResource(const std::string& key, TType*& resource)
{
	auto it = _resources.find(key);
	if (it != _resources.end())
	{
		resource = dynamic_cast<TType*>(it->second);
		return true;
	}
	resource = nullptr;
	return false;
}

inline bool ResourceManager::TryGetResource(const std::string& key, void*& resource)
{
	auto it = _resources.find(key);
	if (it != _resources.end())
	{
		resource = it->second;
		return true;
	}
	resource = nullptr;
	return false;
}
