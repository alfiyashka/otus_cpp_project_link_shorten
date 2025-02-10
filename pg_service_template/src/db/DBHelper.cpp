#include "DBHelper.hpp"
#include "../exceptions/DBException.hpp"
#include "../exceptions/InternalException.hpp"
#include "../ConfigParameters.hpp"

void DBHelper::prepareDB(const bool needReCreate /*= false*/) {
  try
  {
     if (needReCreate) {
      const userver::storages::postgres::Query dropTableQuery{
          DROP_LISKSTORE, userver::storages::postgres::Query::Name{"drop table LISKSTORE"}};
      m_pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                          dropTableQuery);
    }

    const userver::storages::postgres::Query createTableQuery{
        CREATE_LISKSTORE,
        userver::storages::postgres::Query::Name{"create table LISKSTORE"}};
    m_pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                        createTableQuery);

    if (needReCreate) {
      const userver::storages::postgres::Query dropTableLinkRetryQuery{
          DROP_LISKSTORELOGGER, userver::storages::postgres::Query::Name{"drop table LINKLOGGER"}};
      m_pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                          dropTableLinkRetryQuery);
    }

    const userver::storages::postgres::Query createTableLinkRetryQuery{
        CREATE_LISKSTORELOGGER,
        userver::storages::postgres::Query::Name{"create table LINKLOGGER"}};
    m_pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                        createTableLinkRetryQuery);
  }
  catch(const std::exception& e)
  {
    const std::string errorMess = std::string("Cannot prepare tables.") + e.what();
    throw DBException(errorMess.c_str());
  }
}

void DBHelper::prepareSettingsTable(const bool needReCreate /*= false*/) const {
  try
  {
    if (needReCreate) {
      const userver::storages::postgres::Query dropTableSettingsQuery{
          DROP_SETTINGS, userver::storages::postgres::Query::Name{"drop table service_settings"}};
      m_pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                          dropTableSettingsQuery);
    }

    const userver::storages::postgres::Query createTableSettingsQuery{
        CREATE_SETTINGS,
        userver::storages::postgres::Query::Name{"create table service_settings"}};
    m_pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                        createTableSettingsQuery);
  }
  catch(const std::exception& e)
  {
    const std::string errorMess = std::string("Cannot prepare setting table.") + e.what();
    throw DBException(errorMess.c_str());
  }
}

void DBHelper::saveTokenInfo(const std::string& token, const std::string& longUrl) const {
  if (token.empty()) {
    throw InternalLogicException("Cannot save token info: Internal error. Long link's token is empty");
  }
  try
  {
    userver::storages::postgres::Transaction transaction = m_pg_cluster->Begin(
        "transaction_insert_link_with_token_info",
        userver::storages::postgres::ClusterHostType::kMaster, {});

    const userver::storages::postgres::Query kInsertValue{
        "INSERT INTO linkstore (token, link, create_time) "
        "VALUES ($1, $2, current_timestamp) "
        "ON CONFLICT DO NOTHING",
        userver::storages::postgres::Query::Name{"insert_value_link_with_token"},
    };
    transaction.Execute(kInsertValue, token, longUrl);
    transaction.Commit();
  }
  catch(const std::exception& e)
  {
    const std::string errorMess = std::string("Cannot register long url into database.") + e.what();
    throw DBException(errorMess.c_str());
  }
}

std::optional<std::string> DBHelper::findToken(const std::string& longUrl) const {
  try
  {
    const userver::storages::postgres::Query kFindTokenValue{
        "select token from linkstore where link = $1",
        userver::storages::postgres::Query::Name{
            "try_find_token_by_long_url_value"},
    };

    const auto res =
        m_pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kSlave,
                            kFindTokenValue, longUrl);
    if (!res.IsEmpty()) {
      return std::string{res.AsSingleRow<std::string>()};
    }
    return std::nullopt;
  }
  catch(const std::exception& e)
  {
    const std::string errorMess = std::string("Cannot find token of long url from database.") + e.what();
    throw DBException(errorMess.c_str());
  }
}

std::string DBHelper::getLongUrl(const std::string& token) const {
  try
  {
    const userver::storages::postgres::Query kFindTokenValue{
        "select link from linkstore where token = $1",
        userver::storages::postgres::Query::Name{
            "try_find_long_url_by_token_value"},
    };

    const auto res =
        m_pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kSlave,
                            kFindTokenValue, token);
    if (!res.IsEmpty()) {
      return res.AsSingleRow<std::string>();
    }
    return "";
  }
  catch(const std::exception& e)
  {
    const std::string errorMess = std::string("Cannot find long url from database.") + e.what();
    throw DBException(errorMess.c_str());
  }
}

void DBHelper::deleteLongUrlInfo(const std::string& token) const
{
  if (token.empty())
  {
    throw InternalLogicException("Internal error in delete long url indo. Long link's token is empty");
  }
  try
  {
    userver::storages::postgres::Transaction transaction = m_pg_cluster->Begin(
        "sample_transaction_delete_longlink_value",
        userver::storages::postgres::ClusterHostType::kMaster, {});

    const userver::storages::postgres::Query kDeleteValue{
        "Delete from linkstore where token = $1 ",
        userver::storages::postgres::Query::Name{
            "delete_value_link_with_token"},
    };
    const auto deleteRes = transaction.Execute(kDeleteValue, token);
    transaction.Commit();
  }
  catch (const std::exception& e)
  {
    const std::string errorMess = std::string("Cannot clear data about long url from database.") + e.what();
    throw DBException(errorMess.c_str());
  }
}

void DBHelper::cleanExpiredData()
{
  const std::string expiredTimestampParameter 
    = ConfigParametersMap.at(ConfigParametersEnum::expired_token_timestamp);
  const int value = std::atoi(getSettingValue(expiredTimestampParameter).c_str());
  if (value == std::chrono::seconds(0).count())
  {
    LOG_WARNING() << "Ignored cleaning expired data becuase expired_timestamp was undefined";
    return;
  }
  try
  {
    userver::storages::postgres::Transaction transaction = m_pg_cluster->Begin(
        "sample_transaction_delete_expired",
        userver::storages::postgres::ClusterHostType::kMaster, {});

    const userver::storages::postgres::Query kDeleteValue{
        "delete from linkstore where extract(epoch from (current_timestamp - create_time)) >= $1 ",
        userver::storages::postgres::Query::Name{"delete_expired_value" },
    };
    transaction.Execute(kDeleteValue, value);
    transaction.Commit();
  }
  catch (const std::exception& e)
  {
    const std::string errorMess = std::string("Cannot clear expired data from database.") + e.what();
    throw DBException(errorMess.c_str());
  }
}

std::string DBHelper::getSettingValue(const std::string& setting_name) const {
  try
  {
    const userver::storages::postgres::Query kFindSettingValueValue{
        "select value from service_settings where name = $1",
        userver::storages::postgres::Query::Name{
            "try_find_setting_value"},
    };

    const auto res =
        m_pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kSlave,
                            kFindSettingValueValue, setting_name);
    if (!res.IsEmpty()) {
      return res.AsSingleRow<std::string>();
    }
    return "";
  }
  catch(const std::exception& e)
  {
    const std::string errorMess = std::string("Cannot find value of setting  '") + setting_name + "' from database." + e.what();
    throw DBException(errorMess.c_str());
  }
}

void DBHelper::saveSettings(const std::string& name, const std::string& value) const
{
  if (name.empty() || value.empty())
  {
    throw InternalLogicException("Internal error in trying to save settings into. Setting name or value is undefined");
  }
  try
  { 
    userver::storages::postgres::Transaction transaction = m_pg_cluster->Begin(
        "sample_transaction_insert_settings",
        userver::storages::postgres::ClusterHostType::kMaster, {});

    const userver::storages::postgres::Query kInsertValue{
        "INSERT INTO service_settings (name, value) "
        "VALUES ($1, $2) "
        "ON CONFLICT(name) DO UPDATE"
        " SET value = excluded.value",
        userver::storages::postgres::Query::Name{"insert_or_update_value_of_setting"},
    };
    transaction.Execute(kInsertValue, name, value);
    transaction.Commit();
  }
  catch(const std::exception& e)
  {
    const std::string errorMess = std::string("Cannot save save setting info into database.") + e.what();
    throw DBException(errorMess.c_str());
  }
  
}

void DBHelper::saveRequestResult(
  const std::string& token,
  const std::string& longUrlFind,
  const int request_timeout_second, 
  const int request_attempt,
  const int request_code,
  const std::string& error) const
{
  try
  { 
    userver::storages::postgres::Transaction transaction = m_pg_cluster->Begin(
        "sample_transaction_insert_log_info",
        userver::storages::postgres::ClusterHostType::kMaster, {});

    const userver::storages::postgres::Query kInsertValue{
        "INSERT INTO linkstorelogger (request_perform_time, token, longUrl, request_timeout, request_attempt, request_code, error) "
        "VALUES (current_timestamp, $1, $2, $3, $4, $5, $6) ",
        userver::storages::postgres::Query::Name{"insert_or_log_info"},
    };
    transaction.Execute(kInsertValue, token, longUrlFind, request_timeout_second,
      request_attempt, request_code, error);
    transaction.Commit();
  }
  catch(const std::exception& e)
  {
    const std::string errorMess = std::string("Cannot save log info into database.") + e.what();
    throw DBException(errorMess.c_str());
  }
}

