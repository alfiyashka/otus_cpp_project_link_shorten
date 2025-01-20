#include "ShortLinkServer.hpp"

#include <fmt/format.h>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/assert.hpp>
#include <userver/clients/http/component.hpp>

#include "token_gen/TokenGenerator.hpp"

#include <userver/clients/http/client.hpp>

#include <string>

namespace pg_service_template {

namespace {

class ShortLink final : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-url-shorten";

  ShortLink(const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        http_client_(component_context.FindComponent<userver::components::HttpClient>().GetHttpClient()),
        pg_cluster_(
            component_context
                .FindComponent<userver::components::Postgres>("postgres-db-1")
                .GetCluster()) 
  {
    prepare_db();
    
  }

   

  void prepare_db()
  {
      const userver::storages::postgres::Query dropTableQuery{
      "drop table linkstore;",
      userver::storages::postgres::Query::Name{"create tables"}
    };
    pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kMaster, 
     dropTableQuery);

    const userver::storages::postgres::Query createTableQuery{
      "create table if not exists linkstore(token varchar(200), link text);",
      userver::storages::postgres::Query::Name{"create tables"}
    };
    pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kMaster, 
     createTableQuery);
  }

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& context) const override {
    
    
    request.GetHttpResponse().SetContentType(userver::http::content_type::kTextPlain);
    switch (request.GetMethod()) {
        case userver::server::http::HttpMethod::kGet:
        {
          
          return GetValue(request);
        }
        case userver::server::http::HttpMethod::kPut:
        {
          if (request.PathArgCount() == 2 
             && request.GetPathArg(1) == "shorten") // v1/shorten
          {
            return PutValue(request);
          }          
        }
      //  case userver::server::http::HttpMethod::kDelete:
      //      return DeleteValue(key);
        default:
            throw userver::server::handlers::ClientError(userver::server::handlers::ExternalBody{
                fmt::format("Unsupported method {}", request.GetMethod())});
    }
    

    return "";
  }

  userver::storages::postgres::ClusterPtr pg_cluster_;
private:
  std::string PutValue(const userver::server::http::HttpRequest& request) const;
  std::string GetValue(const userver::server::http::HttpRequest& request) const;

  userver::clients::http::Client& http_client_;
};

}  // namespace

std::string ShortLink::PutValue(const userver::server::http::HttpRequest& request) const {
    
    const auto& longUrl = request.RequestBody();

    const userver::storages::postgres::Query kFindTokenValue{
      "select token from linkstore where link = $1",
      userver::storages::postgres::Query::Name{"try_find_token_by_long_url_value"},
    }; 
 
    const auto res = pg_cluster_->Execute(userver::storages::postgres::ClusterHostType::kSlave,
       kFindTokenValue, longUrl);
    if (!res.IsEmpty()) {
        return std::string{"url is exist"};
    }
    else
    {
      try
      {
        const auto token = TokenGenerator::generateToken();
        userver::storages::postgres::Transaction transaction = pg_cluster_->Begin(
            "sample_transaction_insert_key_value",
            userver::storages::postgres::ClusterHostType::kMaster, {});

        const userver::storages::postgres::Query kInsertValue{
            "INSERT INTO linkstore (token, link) "
            "VALUES ($1, $2) "
            "ON CONFLICT DO NOTHING",
            userver::storages::postgres::Query::Name{"insert_value_link_with_token"},
        };
        const auto resInsert = transaction.Execute(kInsertValue, token, longUrl);
        if (resInsert.RowsAffected()) {
          transaction.Commit();
          request.SetResponseStatus(userver::server::http::HttpStatus::kCreated);
          return std::string{"generated url : http://localhost:8088/" + token + "\n"};
        }
      }
      catch(const std::exception& e)
      {
        const auto err = e.what();
        int nn = 0;
      }
         
    } 
    return "";
}

std::string ShortLink::GetValue(const userver::server::http::HttpRequest& request) const {
    
    const auto paths = request.PathArgCount();
    if (paths != 1)
    {
      return request.GetHttpResponse().GetData();
    }

    if (request.HasPathArg(0))
    {
      const auto token = request.GetPathArg(0);
      const userver::storages::postgres::Query kFindTokenValue{
        "select link from linkstore where token = $1",
        userver::storages::postgres::Query::Name{"try_find_long_url_by_token_value"},
      };
  
      const auto res = pg_cluster_->Execute(
          userver::storages::postgres::ClusterHostType::kSlave, kFindTokenValue,
          token);
      if (!res.IsEmpty()) {
        const auto longLinkUrl = res.AsSingleRow<std::string>();

        const auto responce = http_client_.CreateRequest()
          .get(longLinkUrl)
          .timeout(std::chrono::seconds(1))
          .headers(request.GetHeaders())
          .perform();

        if (!responce.get())
        {
          return "unknown result";
        }
        else
        {
          return responce->body();
        }
      }
    }
    return request.GetHttpResponse().GetData();;

}




void AppendShortLink(userver::components::ComponentList& component_list) {
  component_list.Append<ShortLink>();
  component_list.Append<userver::components::Postgres>("postgres-db-1");
  component_list.Append<userver::clients::dns::Component>();
  component_list.Append<userver::components::HttpClient>();
}

}  // namespace pg_service_template
