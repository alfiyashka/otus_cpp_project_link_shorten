#include "ShortLinkServer.hpp"

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

#include "token_gen/TokenGenerator.hpp"

#include <userver/clients/http/client.hpp>

#include <string>

#include "exceptions/DBException.hpp"
#include "exceptions/InternalException.hpp"
#include "ConfigParameters.hpp"

namespace pg_service_template {


ShortLink::ShortLink(const userver::components::ComponentConfig& config,
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


std::string ShortLink::HandleRequestThrow(
  const userver::server::http::HttpRequest& request,
  userver::server::request::RequestContext& ) const 
{
  try
  {
    request.GetHttpResponse().SetContentType(userver::http::content_type::kTextPlain);
    switch (request.GetMethod())
    {
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
        request.SetResponseStatus(userver::server::http::HttpStatus::BadRequest);
        return fmt::format("Unsupported method {}", request.GetMethod());
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

std::string ShortLink::PutValue(const userver::server::http::HttpRequest& request) const
{    
  const auto& longUrl = request.RequestBody();

  auto tokenExist = m_dbHelper.findToken(longUrl);
  if (tokenExist.has_value()) 
  {
    request.SetResponseStatus(userver::server::http::HttpStatus::kFound);
    return std::string{"url is already exists: http://localhost:8088/v1/shorten/" +
                           tokenExist.value() + "\n"};
  } 
  else 
  {
    const auto token = TokenGenerator::generateToken();
    m_dbHelper.saveTokenInfo(token, longUrl);
    request.SetResponseStatus(userver::server::http::HttpStatus::kCreated);
    return std::string{"generated url : http://localhost:8088/v1/shorten/" + token +
                           "\n"};
  }    
}

bool ShortLink::isFailRequestCode(const uint16_t code) const
{
  return code > 200 || code >= 400;
}

std::string ShortLink::GetValue(const userver::server::http::HttpRequest& request) const
{    
  const auto paths = request.PathArgCount();
  if (paths != 1)
  {
    return request.GetHttpResponse().GetData();
  }

  if (request.PathArgCount() == 2 
    && request.GetPathArg(1) == "shorten") // v1/shorten
  {
    const auto token = request.GetPathArg(0);
    const auto longUrlFind = m_dbHelper.getLongUrl(token);
    if (!longUrlFind.empty())
    {
      const auto responce = http_client_.CreateRequest()
        .get(longUrlFind)
        .timeout(std::chrono::seconds(1))
        .headers(request.GetHeaders())
        .perform();
      request.SetResponseStatus(responce->status_code());
      if (responce.get())
      {
        if (isFailRequestCode(responce->status_code()))
        {
          const auto request_wait_timeout 
            = std::atoi(m_dbHelper.getSettingValue(ConfigParametersMap.at(ConfigParametersEnum::request_wait_timeout)).c_str());

          const auto request_retry_attempt
            = std::atoi(m_dbHelper.getSettingValue(ConfigParametersMap.at(ConfigParametersEnum::request_try_attempt)).c_str());

          const auto responceRetry = http_client_.CreateRequest()
            .get("http://localhost:8089/v1/retry/" + token)
            .timeout(std::chrono::seconds(request_wait_timeout * 1000))
            .retry(request_retry_attempt)
            .headers(request.GetHeaders())
            .perform();

            
          m_dbHelper.saveRequestResult(token, longUrlFind, request_wait_timeout, 
            request_retry_attempt, responceRetry->status_code(),
            isFailRequestCode(responceRetry->status_code()) ? responceRetry->body() : "");
          
          request.SetResponseStatus(responceRetry->status_code());
          if (isFailRequestCode(responceRetry->status_code()))
          {            
            return "unknown result from long url : " + longUrlFind + ". Retry request result:'" + responceRetry->body() + "' \n";
          }
          else
          {
            responceRetry->body();
          }
        }
        else
        {
          m_dbHelper.saveRequestResult(token, longUrlFind, 0, 1, responce->status_code(),
            isFailRequestCode(responce->status_code()) ? responce->body() : "");
        }
        return responce->body();
      }
      else
      {
        request.SetResponseStatus(userver::server::http::HttpStatus::NotFound);
        m_dbHelper.saveRequestResult(token, longUrlFind, 0, 1, 
         request.GetHttpResponse().GetStatus(), responce->body());
        return "undefined result";
      }        
    }
    else
    {
      const std::string error = "A short url was expired or unknown\n";
      request.SetResponseStatus(
        userver::server::http::HttpStatus::NotFound);
      m_dbHelper.saveRequestResult(token, "not found long url", 0, 1, 
        request.GetHttpResponse().GetStatus(), error);
      return error;
    }
  }
  return request.GetHttpResponse().GetData();

}


std::string ShortLink::DeleteValue(const userver::server::http::HttpRequest& request) const
{
  if (request.HasPathArg(0))
  {
    const auto token = request.GetPathArg(0);
    m_dbHelper.deleteLongUrlInfo(token);
    request.SetResponseStatus(userver::server::http::HttpStatus::kAccepted);
    return "";
      
  }

  return "unknown url for delete request";
  request.SetResponseStatus(userver::server::http::HttpStatus::BadRequest);
}

void AppendShortLink(userver::components::ComponentList& component_list) {
  component_list.Append<ShortLink>();
  component_list.Append<userver::components::Postgres>("postgres-db-1");
  component_list.Append<userver::clients::dns::Component>();
  component_list.Append<userver::components::HttpClient>();  
}

}  // namespace pg_service_template
