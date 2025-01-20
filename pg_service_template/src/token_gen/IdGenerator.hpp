#ifndef _ID_GENERATOR_HPP_
#define _ID_GENERATOR_HPP_

#include <mutex>
#include <atomic>
#include  <cstring>
/**
 * Singleton. Helps to auto generate unique id for each data 
 */
class IDGenerator
{    
public:
    static IDGenerator* getGenerator();
    std::size_t generateId();

private:
    IDGenerator()
    {
        m_id.store(0);
    }
    ~IDGenerator() = default;
    IDGenerator(const IDGenerator& ) = delete;
    IDGenerator& operator=(const IDGenerator&) = delete;
    static std::atomic<std::size_t> m_id;

    static std::atomic<IDGenerator*> m_instance;
    static std::mutex m_mutex;
};

#endif