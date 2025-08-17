#include "ResourceManager.hpp"

namespace Grace
{

ResourceManager::ResourceManager(uint32_t framesInFlight)
{
    m_DeletionQueue = std::make_unique<DeletionQueue>(framesInFlight);
}

void ResourceManager::FlushDeletionQueue(uint32_t frameIndex)
{
    m_DeletionQueue->Flush(frameIndex);
}

std::vector<RegistryEntry<Image>>& ResourceManager::GetAllImages()
{
    return m_ImagesRegistry.GetAll();
}

} // namespace Grace
