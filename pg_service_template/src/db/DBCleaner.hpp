#ifndef __DB_CLEANER_HPP__
#define __DB_CLEANER_HPP__

#include <userver/utils/periodic_task.hpp>
#include <chrono>
#include "DBHelper.hpp"
#include <iostream>


class DBCleaner
{
private:

    userver::utils::PeriodicTask m_cleaner;
    std::chrono::seconds m_clean_period;
    std::chrono::seconds m_expired_timestamp;
    DBHelper m_dbHelper;
    inline static const std::chrono::seconds DEFAULT_CLEAN_PERIOD = std::chrono::seconds(3);//(10);
    inline static const std::chrono::seconds DEFAULT_EXPIRED_TIMESTAMP = std::chrono::seconds(10 * 24 * 60); // 10 days
    void CleanExpiredData();

public:
    DBCleaner(DBHelper& dbHelper);

    DBCleaner(DBHelper& dbHelper
        , const std::chrono::seconds expired_timestamp
        , const std::chrono::seconds clean_period);

    void start();
};


#endif