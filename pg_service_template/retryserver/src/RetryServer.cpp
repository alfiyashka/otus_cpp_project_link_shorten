#include "RetryServer.hpp"
#include "../../src/db/DBHelper.hpp"

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

namespace {

class RetryService final : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handle-retry-server";

  RetryService(const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        http_client_(component_context.FindComponent<userver::components::HttpClient>().GetHttpClient()),
        m_dbHelper(
            component_context
                .FindComponent<userver::components::Postgres>("postgres-db-1")
                .GetCluster())
  {
    m_dbHelper.prepareDB(true);
  }  


  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& ) const override {
  
    
    request.GetHttpResponse().SetContentType(userver::http::content_type::kTextPlain);
    switch (request.GetMethod()) {
        case userver::server::http::HttpMethod::kGet:
        {
          return GetValue(request);
        }       
        default:
            throw userver::server::handlers::ClientError(userver::server::handlers::ExternalBody{
                fmt::format("Unsupported method {}", request.GetMethod())});
    }
    

    return "";
  }

  userver::storages::postgres::ClusterPtr pg_cluster_;
private:
  std::string GetValue(const userver::server::http::HttpRequest& request) const;
  userver::clients::http::Client& http_client_;

  DBHelper m_dbHelper;
};

}  // namespace


std::string RetryService::GetValue(const userver::server::http::HttpRequest& request) const {
    
    const auto paths = request.PathArgCount();
    if (paths != 1)
    {
      return request.GetHttpResponse().GetData();
    }

    if (request.HasPathArg(0))
    {
      const auto token = request.GetPathArg(0);
      const auto longUrlFind = m_dbHelper.getLongUrl(token);
      if (!longUrlFind.empty()) {
        const auto responce = http_client_.CreateRequest()
          .get(longUrlFind)
          .timeout(std::chrono::seconds(1))
          .headers(request.GetHeaders())
          .perform();
        request.SetResponseStatus(responce->status_code());
        if (responce.get())
        {
          if (responce->status_code() > 200
            || responce->status_code() >= 300)
          {
            return "unknown result from long url : " + longUrlFind;
          }
          return "";
          
        }
        else
        {
          return responce->body();
        }        
      }
      else
      {
        request.SetResponseStatus(
          userver::server::http::HttpStatus::NotFound);
        return std::string("A short url was expired or unknown\n");
      }
    }
    return request.GetHttpResponse().GetData();

}


void AppendRetryService(userver::components::ComponentList& component_list) {
  component_list.Append<RetryService>();
  component_list.Append<userver::components::Postgres>("postgres-db-1");
  component_list.Append<userver::clients::dns::Component>();
  component_list.Append<userver::components::HttpClient>();
}

}  // namespace pg_service_template
