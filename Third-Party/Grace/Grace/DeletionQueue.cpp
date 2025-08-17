#include "DeletionQueue.hpp"

Grace::DeletionQueue::DeletionQueue(uint32_t framesInFlight)
{
    m_Deleters.resize(framesInFlight);
}

void Grace::DeletionQueue::PushDeleter(std::function<void()>&& deleter, uint32_t frameIndex)
{
    m_Deleters[frameIndex].push_back(deleter);
}

void Grace::DeletionQueue::Flush(uint32_t frameIndex)
{
    for (auto it = m_Deleters[frameIndex].rbegin(); it != m_Deleters[frameIndex].rend(); it++)
    {
        (*it)();
    }
    m_Deleters[frameIndex].clear();
}
