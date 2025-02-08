#include "DBHelper.hpp"
#include "../exceptions/DBException.hpp"
#include "../exceptions/InternalException.hpp"

void DBHelper::prepareDB(const bool needReCreate /*= false*/) {
  try
  {
     if (needReCreate) {
      const userver::storages::postgres::Query dropTableQuery{
          DROP_LISKSTORE, userver::storages::postgres::Query::Name{"drop table"}};
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
          DROP_LINKRETRY, userver::storages::postgres::Query::Name{"drop table"}};
      m_pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                          dropTableLinkRetryQuery);
    }

    const userver::storages::postgres::Query createTableLinkRetryQuery{
        CREATE_LINKRETRY,
        userver::storages::postgres::Query::Name{"create table LINKRETRY"}};
    m_pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                        createTableLinkRetryQuery);
  }
  catch(const std::exception& e)
  {
    const std::string errorMess = std::string("Cannot prepare tables.") + e.what();
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

void DBHelper::cleanExpiredData(const std::chrono::seconds& expired_timestamp)
{
  if (expired_timestamp == std::chrono::seconds(0))
  {
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
    transaction.Execute(kDeleteValue, expired_timestamp.count());
    transaction.Commit();
  }
  catch (const std::exception& e)
  {
    const std::string errorMess = std::string("Cannot clear expired data from database.") + e.what();
    throw DBException(errorMess.c_str());
  }
}

std::tuple<std::string, int> DBHelper::getLongUrlToRetry(const int64_t id) const {
  try
  {
    const userver::storages::postgres::Query kFindLinkValue{
        "select link, retry_attempt from linkretry where id = $1",
        userver::storages::postgres::Query::Name{
            "try_find_long_url_id_value_to_retry"},
    };

    const auto res =
        m_pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kSlave,
                            kFindLinkValue, id);
    if (!res.IsEmpty()) {
      return res.AsSingleRow<std::tuple<std::string, int> >();
    }
    return std::tuple("", -1);
  }
  catch(const std::exception& e)
  {
    const std::string errorMess = std::string("Cannot get long url from database to retry.") + e.what();
    throw DBException(errorMess.c_str());
  }
}

void DBHelper::deleteLongUrlToRetry(const int64_t id) const
{
  if (id < 0) {
    throw InternalLogicException("Internal error in delete. Retry link id is undefined");
  }
  try
  {
    userver::storages::postgres::Transaction transaction = m_pg_cluster->Begin(
        "sample_transaction_delete_longretry_value",
        userver::storages::postgres::ClusterHostType::kMaster, {});

    const userver::storages::postgres::Query kDeleteValue{
        "Delete from linkretry where id = $1 ",
        userver::storages::postgres::Query::Name{"delete_value_retrylink_by_id"},
    };
    const auto deleteRes = transaction.Execute(kDeleteValue, id);
    transaction.Commit();
  }
  catch(const std::exception& e)
  {
    const std::string errorMess = std::string("Cannot delete long url from database.") + e.what();
    throw DBException(errorMess.c_str());
  }
}


void DBHelper::saveLongUrlToRetry(const int64_t id, const std::string& longUrl, const int retryAattempt) const {
  if (id < 0) {
    throw InternalLogicException("Internal error in trying to save long url info. Retry link id is undefined");
  }
  try
  { 
    userver::storages::postgres::Transaction transaction = m_pg_cluster->Begin(
        "sample_transaction_insert_retry_link",
        userver::storages::postgres::ClusterHostType::kMaster, {});

    const userver::storages::postgres::Query kInsertValue{
        "INSERT INTO linkretry (id, link, retry_attempt) "
        "VALUES ($1, $2, $3) "
        "ON CONFLICT DO NOTHING",
        userver::storages::postgres::Query::Name{"insert_value_retry_link"},
    };
    transaction.Execute(kInsertValue, id, longUrl, retryAattempt);
    transaction.Commit();
  }
  catch(const std::exception& e)
  {
    const std::string errorMess = std::string("Cannot save long url into database.") + e.what();
    throw DBException(errorMess.c_str());
  }
  
}


int64_t DBHelper::getMaxIdRetry() const {
  try
  { 
    const userver::storages::postgres::Query kFindIdValue{
        "select coalesce(max(id), 0) as maxId from linkretry",
        userver::storages::postgres::Query::Name{
            "try_find_max_id_value_to_retry"},
    };

    const auto res =
        m_pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kSlave,
                            kFindIdValue);
    if (!res.IsEmpty())
    {
        const auto maxid = res.AsSingleRow<int64_t>();
        return maxid + 1;
    }
    else
    {
      return 1;
    }
  }
  catch(const std::exception& e)
  {
    const std::string errorMess = std::string("Cannot get  max id of long url from database.") + e.what();
    throw DBException(errorMess.c_str());
  }
}