#pragma once

#include <userver/components/minimal_server_component_list.hpp>
#include <userver/rcu/rcu.hpp>
#include <userver/server/handlers/http_handler_json_base.hpp>
#include <userver/utils/daemon_run.hpp>
#include <userver/utils/datetime.hpp>
 
#include <userver/formats/json.hpp>
 
#include <userver/utest/using_namespace_userver.hpp>

#include "db/DBHelper.hpp"

namespace pg_service_template {

 
class ConfigDistributor final : public server::handlers::HttpHandlerJsonBase {
public:
    static constexpr std::string_view kName = "handler-config-parameter1";
 
    using KeyValues = std::unordered_map<std::string, formats::json::Value>;
 
    // Component is valid after construction and is able to accept requests
    ConfigDistributor(const components::ComponentConfig& config, const components::ComponentContext& context);
 
    formats::json::Value
    HandleRequestJsonThrow(const server::http::HttpRequest&, const formats::json::Value& json, server::request::RequestContext&)
        const override;
 
 
private:
    DBHelper m_dbHelper;
    
};
 
 
 
}
