#pragma once

#include <userver/components/minimal_server_component_list.hpp>
#include <userver/rcu/rcu.hpp>
#include <userver/server/handlers/http_handler_json_base.hpp>
#include <userver/utils/daemon_run.hpp>
#include <userver/utils/datetime.hpp>
 
#include <userver/formats/json.hpp>
 
#include <userver/utest/using_namespace_userver.hpp>

namespace pg_service_template {

struct ConfigDataWithTimestamp {
    std::chrono::system_clock::time_point updated_at;
    std::unordered_map<std::string, formats::json::Value> key_values;
};
 
class ConfigDistributor final : public server::handlers::HttpHandlerJsonBase {
public:
    static constexpr std::string_view kName = "handler-config-parameter1";
 
    using KeyValues = std::unordered_map<std::string, formats::json::Value>;
 
    // Component is valid after construction and is able to accept requests
    ConfigDistributor(const components::ComponentConfig& config, const components::ComponentContext& context);
 
    formats::json::Value
    HandleRequestJsonThrow(const server::http::HttpRequest&, const formats::json::Value& json, server::request::RequestContext&)
        const override;
 
    void SetNewValues(KeyValues&& key_values) {
        config_values_.Assign(ConfigDataWithTimestamp{
            /*.updated_at=*/utils::datetime::Now(),
            /*.key_values=*/std::move(key_values),
        });
    }
 
private:
    rcu::Variable<ConfigDataWithTimestamp> config_values_;
};
 
 
formats::json::ValueBuilder
MakeConfigs(const rcu::ReadablePtr<ConfigDataWithTimestamp>& config_values_ptr, const formats::json::Value& request);
 

 
}
