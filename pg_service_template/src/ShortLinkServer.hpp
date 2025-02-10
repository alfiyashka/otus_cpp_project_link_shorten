#pragma once

#include <string>
#include <string_view>

#include <userver/components/component_list.hpp>
#include "db/DBHelper.hpp"
#include "db/DBCleaner.hpp"

#include <fmt/format.h>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/assert.hpp>
#include <userver/clients/http/component.hpp>

#include <userver/clients/http/client.hpp>

#include <string>

namespace pg_service_template {

class ShortLink final : public userver::server::handlers::HttpHandlerBase
{
public:
  static constexpr std::string_view kName = "handler-url-shorten";

  ShortLink(const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context);

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& ) const override;

  userver::storages::postgres::ClusterPtr pg_cluster_;
private:
  std::string PutValue(const userver::server::http::HttpRequest& request) const;
  std::string GetValue(const userver::server::http::HttpRequest& request) const;
  std::string DeleteValue(const userver::server::http::HttpRequest& request) const;

  bool isFailRequestCode(const uint16_t code) const;

  userver::clients::http::Client& http_client_;

  DBHelper m_dbHelper;
  DBCleaner m_dbCleaner;
};


void AppendShortLink(userver::components::ComponentList& component_list);

}  // namespace pg_service_template
