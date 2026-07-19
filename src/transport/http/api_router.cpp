#include "taskflow/transport/http/api_router.hpp"

#if TASKFLOW_HAS_DROGON
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

namespace taskflow::transport::http {
namespace {

[[nodiscard]] drogon::HttpResponsePtr make_response(const ApiError &error) {
  auto response = drogon::HttpResponse::newHttpResponse();
  response->setStatusCode(static_cast<drogon::HttpStatusCode>(error.status));
  response->setContentTypeString("application/json; charset=utf-8");
  response->setBody(serialize_error(error));
  response->addHeader("X-Request-ID", error.request_id);
  return response;
}

[[nodiscard]] std::string request_id_from(const drogon::HttpRequestPtr &request) {
  const auto stored =
      request->getAttributes()->get<std::string>(std::string{request_id_attribute});
  return stored.empty()
             ? validate_request("GET", 0, "", request->getHeader("x-request-id")).request_id
             : stored;
}

} // namespace

void configure_api_router(drogon::HttpAppFramework &application) {
  application.setClientMaxBodySize(maximum_request_body_bytes)
      .setClientMaxMemoryBodySize(maximum_request_body_bytes)
      .registerSyncAdvice([](const drogon::HttpRequestPtr &request) {
        const auto validation =
            validate_request(request->methodString(), request->bodyLength(),
                             request->getHeader("content-type"), request->getHeader("x-request-id"));
        request->getAttributes()->insert(std::string{request_id_attribute}, validation.request_id);
        return validation.error ? make_response(*validation.error) : drogon::HttpResponsePtr{};
      })
      .registerPreSendingAdvice(
          [](const drogon::HttpRequestPtr &request, const drogon::HttpResponsePtr &response) {
            response->addHeader("X-Request-ID", request_id_from(request));
          })
      .setCustomErrorHandler([](const drogon::HttpStatusCode status,
                                const drogon::HttpRequestPtr &request) {
        const int code = static_cast<int>(status);
        return make_response(ApiError{code, code == 404 ? "not_found" : "http_error",
                                      code == 404 ? "resource not found" : "request failed", {},
                                      request_id_from(request)});
      });

  application.registerHandler(
      std::string{api_prefix},
      [](const drogon::HttpRequestPtr &request,
         std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        auto response = drogon::HttpResponse::newHttpResponse();
        response->setStatusCode(drogon::k200OK);
        response->setContentTypeString("application/json; charset=utf-8");
        response->setBody("{\"version\":\"v1\"}");
        response->addHeader("X-Request-ID", request_id_from(request));
        callback(response);
      },
      {drogon::Get});
}

} // namespace taskflow::transport::http
#endif
