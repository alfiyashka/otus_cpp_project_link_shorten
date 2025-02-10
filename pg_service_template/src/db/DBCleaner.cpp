#include "DBCleaner.hpp"
#include "../ConfigParameters.hpp"


DBCleaner::DBCleaner(DBHelper& dbHelper)
    : m_dbHelper(dbHelper){}

void DBCleaner::CleanExpiredData() {
  m_dbHelper.cleanExpiredData();
  std::cout << "Cleaning expired data; " << std::endl;
}

int DBCleaner::getCleanPeriod() const
{
  const int value = std::atoi(m_dbHelper.getSettingValue(
     ConfigParametersMap.at(ConfigParametersEnum::clean_db_period)).c_str());
  if (value == std::chrono::seconds(0).count())
  {
    LOG_WARNING() << "Used default clean expired data period in seconds, because expired data period from config was undefined";
    return DEFAULT_CLEAN_PERIOD.count();
  }
  return value;

}

void DBCleaner::resetSettings()
{
  const std::chrono::seconds period(getCleanPeriod());
  m_cleaner.SetSettings(userver::utils::PeriodicTask::Settings{period});
}

void DBCleaner::start() {
  const std::chrono::seconds period(getCleanPeriod());
  m_cleaner.Start("cleaner_expired_data",
                  userver::utils::PeriodicTask::Settings{period},
                  [this] { CleanExpiredData(); });
  
}

void DBCleaner::stop() {
  m_cleaner.Stop();
}