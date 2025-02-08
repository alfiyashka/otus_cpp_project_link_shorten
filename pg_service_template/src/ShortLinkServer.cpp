#include "ShortLinkServer.hpp"

#include "db/DBHelper.hpp"
#include "db/DBCleaner.hpp"

#include <fmt/format.h>
#include "ConfigHandler.hpp"

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
  static constexpr std::string_view kName = "handler-url-shorten" ; //"handler-url-shorten";

  ShortLink(const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        http_client_(component_context.FindComponent<userver::components::HttpClient>().GetHttpClient()),
        m_dbHelper(
            component_context
                .FindComponent<userver::components::Postgres>("postgres-db-1")
                .GetCluster()),
        m_dbCleaner(m_dbHelper)
  {
    m_dbHelper.prepareDB(true);
    m_dbCleaner.start();
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
        case userver::server::http::HttpMethod::kDelete:
        {
          return DeleteValue(request);
        }          
        case userver::server::http::HttpMethod::kPut:
        {
          if (request.PathArgCount() == 2 
             && request.GetPathArg(1) == "shorten") // v1/shorten
          {
            return PutValue(request);
          }         
        }
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
  std::string DeleteValue(const userver::server::http::HttpRequest& request) const;
  userver::clients::http::Client& http_client_;

  DBHelper m_dbHelper;
  DBCleaner m_dbCleaner;
};

}  // namespace

std::string ShortLink::PutValue(const userver::server::http::HttpRequest& request) const {
    
    const auto& longUrl = request.RequestBody();

    try 
    {
      auto tokenExist = m_dbHelper.findToken(longUrl);
      if (tokenExist.has_value()) 
      {
        request.SetResponseStatus(userver::server::http::HttpStatus::kFound);
        return std::string{"url is already exists: http://localhost:8088/" +
                           tokenExist.value() + "\n"};
      } 
      else 
      {
        const auto token = TokenGenerator::generateToken();
        m_dbHelper.saveTokenInfo(token, longUrl);
        request.SetResponseStatus(userver::server::http::HttpStatus::kCreated);
        return std::string{"generated url : http://localhost:8088/" + token +
                           "\n"};
      }
    } catch (const std::exception& e) 
    {
      request.SetResponseStatus(
          userver::server::http::HttpStatus::InternalServerError);
      return std::string("Cannot generate a short url because of following error: ")
       + e.what() + " \n";
    }
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
            const auto maxId = m_dbHelper.getMaxIdRetry();
            m_dbHelper.saveLongUrlToRetry(maxId, longUrlFind, 1);

            const auto responceRetry = http_client_.CreateRequest()
              .get("http://localhost:8089/v1/retry/" + std::to_string(maxId))
              .timeout(std::chrono::seconds(1))
              .headers(request.GetHeaders())
              .perform();

            if (responceRetry->status_code() > 200
              || responceRetry->status_code() >= 300)
            {
              request.SetResponseStatus(responceRetry->status_code());
              return "unknown result from long url : " + longUrlFind + ". Retry request result:'" + responceRetry->body() + "' \n";
            }
            else
            {
              request.SetResponseStatus(responceRetry->status_code());
              responceRetry->body();
            }
          }
          return responce->body();
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


std::string ShortLink::DeleteValue(const userver::server::http::HttpRequest& request) const {
    
    if (request.HasPathArg(0))
    {
      try {
        const auto token = request.GetPathArg(0);
        m_dbHelper.deleteLongUrlInfo(token);
        request.SetResponseStatus(userver::server::http::HttpStatus::kAccepted);
        return "";
      }
      catch (const std::exception& e)
      {
        request.SetResponseStatus(
          userver::server::http::HttpStatus::InternalServerError);
        return std::string("cannot perform delete request because of following error: ") + e.what() ;
      }
    }
    return "unknown url for delete reqquest";
    request.SetResponseStatus(
          userver::server::http::HttpStatus::BadRequest);

}



void AppendShortLink(userver::components::ComponentList& component_list) {
  component_list.Append<ShortLink>();
  component_list.Append<ConfigDistributor>();
  component_list.Append<userver::components::Postgres>("postgres-db-1");
  component_list.Append<userver::clients::dns::Component>();
  component_list.Append<userver::components::HttpClient>();
}

}  // namespace pg_service_template
