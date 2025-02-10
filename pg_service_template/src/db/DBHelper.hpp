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

    static inline const std::string DROP_LISKSTORELOGGER = "drop table if exists linkstorelogger;";
    static inline const std::string CREATE_LISKSTORELOGGER =
        "create table if not exists linkstorelogger"
        "(request_time timestamp primary key, "
        "token varchar(200),"
        "longUrl text, "
        "request_timeout integer, "
        "request_attempt integer, "
        "request_code integer,"
        "error text);";

    static inline const std::string DROP_SETTINGS = "drop table if exists service_settings;";
    static inline const std::string CREATE_SETTINGS = "create table if not exists service_settings(name varchar(250) primary key, value varchar(200));";


public:
    DBHelper(userver::storages::postgres::ClusterPtr pg_cluster): m_pg_cluster(pg_cluster)
    {
        
    }

    void prepareDB(const bool needReCreate = false);

    void prepareSettingsTable(const bool needReCreate = false) const;

    void saveTokenInfo(const std::string& token, const std::string& longUrl) const;

    std::optional<std::string> findToken(const std::string& longUrl) const;

    std::string getLongUrl(const std::string& token) const;

    void deleteLongUrlInfo(const std::string& token) const;

    void cleanExpiredData();

    std::string getSettingValue(const std::string& setting_name) const;

    void saveSettings(const std::string& name, const std::string& value) const;

    void saveRequestResult(
        const std::string& token,
        const std::string& longUrlFind,
        const int request_timeout_second, 
        const int request_attempt,
        const int request_code,
        const std::string& error) const;

};

#endif