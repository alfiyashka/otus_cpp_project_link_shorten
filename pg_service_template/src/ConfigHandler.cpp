#include "ConfigHandler.hpp"

#include <userver/clients/dns/component.hpp>
#include <userver/components/component.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/assert.hpp>

#include "ConfigParameters.hpp"


namespace pg_service_template {

ConfigDistributor::ConfigDistributor(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context)
  : HttpHandlerJsonBase(config, component_context),
    m_dbHelper(component_context.FindComponent<userver::components::Postgres>("postgres-db-1").GetCluster()) 
{
    m_dbHelper.prepareSettingsTable();
 
    formats::json::ValueBuilder json;
    json[ConfigParametersMap.at(ConfigParametersEnum::request_wait_timeout)] = "10";
    json[ConfigParametersMap.at(ConfigParametersEnum::request_try_attempt)] = "3";
    json[ConfigParametersMap.at(ConfigParametersEnum::clean_db_period)] = "10";
    json[ConfigParametersMap.at(ConfigParametersEnum::expired_token_timestamp)] = "60";

    for (auto [key, value] : Items(json.ExtractValue()))
    {
      m_dbHelper.saveSettings(key, value.As<std::string>());
    }
}



formats::json::Value ConfigDistributor::
    HandleRequestJsonThrow(const server::http::HttpRequest& request,
    const formats::json::Value& json, server::request::RequestContext&)  const
{
    formats::json::ValueBuilder result;

    try
    {
      for (auto [key, value] : Items(json)) {   
        m_dbHelper.saveSettings(key, value.As<std::string>());
      }  
      result["result"] = "success";
      result["error"] = "";
    }
    catch(const std::exception& e)
    {
      result["result"] = "failure";
      result["error"] = e.what();
    }
    request.GetHttpResponse().SetContentType(http::content_type::kApplicationJson);
    return result.ExtractValue();
}

}