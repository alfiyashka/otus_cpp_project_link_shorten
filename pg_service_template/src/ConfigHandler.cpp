#include "ConfigHandler.hpp"

namespace pg_service_template {

ConfigDistributor::ConfigDistributor(
    const components::ComponentConfig& config,
    const components::ComponentContext& context
) : HttpHandlerJsonBase(config, context) {

    constexpr std::string_view kDynamicConfig = R"~({
    "BAGGAGE_SETTINGS": {
      "allowed_keys": []
    },
    "USERVER_BAGGAGE_ENABLED": false,
    "USERVER_TASK_PROCESSOR_PROFILER_DEBUG": {},
    "USERVER_LOG_REQUEST": true,
    "USERVER_LOG_REQUEST_HEADERS": false,
    "USERVER_CANCEL_HANDLE_REQUEST_BY_DEADLINE": false,
    "USERVER_RPS_CCONTROL_CUSTOM_STATUS": {},
    "USERVER_HTTP_PROXY": "",
    "USERVER_TASK_PROCESSOR_QOS": {
      "default-service": {
        "default-task-processor": {
          "wait_queue_overload": {
            "action": "ignore",
            "length_limit": 5000,
            "time_limit_us": 3000
          }
        }
      }
    },
    "USERVER_CACHES": {},
    "USERVER_LRU_CACHES": {},
    "USERVER_DUMPS": {}
  })~";
 
    auto json = formats::json::FromString(kDynamicConfig);
 
    KeyValues new_config;
    for (auto [key, value] : Items(json)) {
        new_config[std::move(key)] = value;
    }
 
    new_config["USERVER_LOG_REQUEST_HEADERS"] = formats::json::ValueBuilder(true).ExtractValue();
 
    SetNewValues(std::move(new_config));
}

formats::json::ValueBuilder
MakeConfigs(const rcu::ReadablePtr<ConfigDataWithTimestamp>& config_values_ptr, const formats::json::Value& request) {
    formats::json::ValueBuilder configs(formats::common::Type::kObject);
 
    const auto updated_since = request["updated_since"].As<std::string>({});
    if (!updated_since.empty() && utils::datetime::Stringtime(updated_since) >= config_values_ptr->updated_at) {
        // Return empty JSON if "updated_since" is sent and no changes since then.
        return configs;
    }
 
    LOG_DEBUG() << "Sending dynamic config for service " << request["service"].As<std::string>("<unknown>");
 
    const auto& values = config_values_ptr->key_values;
    if (request["ids"].IsMissing()) {
        // Sending all the configs.
        for (const auto& [key, value] : values) {
            configs[key] = value;
        }
 
        return configs;
    }
 
    // Sending only the requested configs.
    for (const auto& id : request["ids"]) {
        const auto key = id.As<std::string>();
 
        const auto it = values.find(key);
        if (it != values.end()) {
            configs[key] = it->second;
        } else {
            LOG_ERROR() << "Failed to find config with name '" << key << "'";
        }
    }
 
    return configs;
}


formats::json::Value ConfigDistributor::
    HandleRequestJsonThrow(const server::http::HttpRequest& request, const formats::json::Value& json, server::request::RequestContext&)
        const {
    formats::json::ValueBuilder result;
 
    const auto config_values_ptr = config_values_.Read();
    result["configs"] = MakeConfigs(config_values_ptr, json);
 
    const auto updated_at = config_values_ptr->updated_at;
    result["updated_at"] = utils::datetime::Timestring(updated_at);
    request.GetHttpResponse().SetContentType(http::content_type::kApplicationJson);
    return result.ExtractValue();
}

}