#include "RetryServer.hpp"
#include "../../src/db/DBHelper.hpp"
#include "../../src/exceptions/DBException.hpp"
#include "../../src/exceptions/InternalException.hpp"

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

RetryService::RetryService(const userver::components::ComponentConfig& config,
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


std::string RetryService::HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& ) const 
{
  try
  {
    request.GetHttpResponse().SetContentType(userver::http::content_type::kTextPlain);
    switch (request.GetMethod()) {
        case userver::server::http::HttpMethod::kGet:
        {
          if (request.PathArgCount() == 3  //  v1/retry/<token>
            && request.GetPathArg(0) == "v1"
            && request.GetPathArg(1) == "retry")
          {
            return GetValue(request);
          }          
        }       
        default:
            throw userver::server::handlers::ClientError(userver::server::handlers::ExternalBody{
                fmt::format("Unsupported method {}", request.GetMethod())});
    }
  }
  catch (const DBException& e)
  {
    request.SetResponseStatus(userver::server::http::HttpStatus::Invalid);
    return std::string("PostgreSQL DB error") + e.what();
  }
  catch (const InternalLogicException& e)
  {
    request.SetResponseStatus(userver::server::http::HttpStatus::InternalServerError);
    return std::string("Internal logic error") + e.what();
  }
  catch (const std::exception& e)
  {
    request.SetResponseStatus(userver::server::http::HttpStatus::InternalServerError);
    return std::string("Internal logic error") + e.what();
  }
}



std::string RetryService::GetValue(const userver::server::http::HttpRequest& request) const {
    
  const auto token = request.GetPathArg(2).c_str();

  const auto longUrl = m_dbHelper.getLongUrl(token);
  if (!longUrl.empty())
  {
    const auto responce = http_client_.CreateRequest()
                              .get(longUrl)
                              .headers(request.GetHeaders())
                              .perform();

    request.SetResponseStatus(responce->status_code());
    if (responce.get())
    {
      if (responce->status_code() > 200 || responce->status_code() >= 400)
      {
        return std::string("request with url : ") + longUrl + " is failed.\n ";
      }
      else
      {
        return responce->body();
      }
    }
    return responce->body();
  }
  else
  {
    request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
    return "url's token was expired";
  }

}


void AppendRetryService(userver::components::ComponentList& component_list) {
  component_list.Append<RetryService>();
  component_list.Append<userver::components::Postgres>("postgres-db-1");
  component_list.Append<userver::clients::dns::Component>();
  component_list.Append<userver::components::HttpClient>();
}

}  // namespace pg_service_template
