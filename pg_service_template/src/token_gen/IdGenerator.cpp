#include "IdGenerator.hpp"

std::atomic<IDGenerator*> IDGenerator::m_instance;

std::mutex IDGenerator::m_mutex;

std::atomic<std::size_t> IDGenerator::m_id = 0;

IDGenerator *IDGenerator::getGenerator()
{
    IDGenerator *idGen = m_instance.load();
    if (!idGen)
    {
        std::lock_guard<std::mutex> myLock(m_mutex);
        idGen = m_instance.load(std::memory_order_relaxed);
        if (!idGen)
        {
            idGen = new IDGenerator();
            m_instance.store(idGen);
        }
    }
    volatile int dummy{};
    return idGen;
}
std::size_t IDGenerator::generateId()
{
    m_id++;
    return m_id.load(std::memory_order_relaxed);
}


