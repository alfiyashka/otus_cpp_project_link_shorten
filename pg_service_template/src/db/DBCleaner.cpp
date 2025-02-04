#include "DBCleaner.hpp"


DBCleaner::DBCleaner(DBHelper& dbHelper)
    : m_dbHelper(dbHelper),
      m_clean_period(DEFAULT_CLEAN_PERIOD),
      m_expired_timestamp(DEFAULT_EXPIRED_TIMESTAMP) {}

DBCleaner::DBCleaner(DBHelper& dbHelper,
                     const std::chrono::seconds expired_timestamp,
                     const std::chrono::seconds clean_period)
    : m_dbHelper(dbHelper),
      m_clean_period(clean_period),
      m_expired_timestamp(expired_timestamp) {}

void DBCleaner::CleanExpiredData() {
  m_dbHelper.cleanExpiredData(m_expired_timestamp);
  std::cout << "Cleaning expired data; " << std::endl;
}

void DBCleaner::start() {
  m_cleaner.Start("cleaner_expired_data",
                  userver::utils::PeriodicTask::Settings{m_clean_period},
                  [this] { CleanExpiredData(); });
}
