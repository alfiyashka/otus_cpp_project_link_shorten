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
    DBHelper m_dbHelper;
    inline static const std::chrono::seconds DEFAULT_CLEAN_PERIOD = std::chrono::seconds(10);//(10);
    void CleanExpiredData();

    int getCleanPeriod() const;

    void resetSettings();

public:
    DBCleaner(DBHelper& dbHelper);

    
    void start();
    void stop();
};


#endif