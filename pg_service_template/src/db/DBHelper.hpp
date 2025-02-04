#ifndef __DB_HELPER__
#define __DB_HELPER__

#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <chrono>

#include <string>

class DBHelper
{
    userver::storages::postgres::ClusterPtr m_pg_cluster;
    static inline const std::string DROP_LISKSTORE = "drop table if exists linkstore;";
    static inline const std::string CREATE_LISKSTORE = "create table if not exists linkstore(token varchar(200) primary key, link text, create_time timestamp);";

    static inline const std::string DROP_LINKRETRY = "drop table if exists linkretry;";
    static inline const std::string CREATE_LINKRETRY = "create table if not exists linkretry(id bigint primary key, link text, retry_attempt int);";

public:
    DBHelper(userver::storages::postgres::ClusterPtr pg_cluster): m_pg_cluster(pg_cluster)
    {
        
    }

    void prepareDB(bool needReCreate = false);

    void saveTokenInfo(const std::string& token, const std::string& longUrl) const;

    std::optional<std::string> findToken(const std::string& longUrl) const;

    std::string getLongUrl(const std::string& token) const;

    void deleteLongUrlInfo(const std::string& token) const;

    void cleanExpiredData(const std::chrono::seconds& expired_timestamp);

    void saveLongUrlToRetry(const int64_t id, const std::string& longUrl, const int retryAattempt) const;

    std::tuple< std::string, int> getLongUrlToRetry(const int64_t id) const;

    void deleteLongUrlToRetry(const int64_t id) const;


};

#endif