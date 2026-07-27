#pragma once

#include "taskflow/infrastructure/notification_delivery.hpp"
#include "taskflow/infrastructure/project_repositories.hpp"
#include "taskflow/transport/websocket/gateway.hpp"

#if TASKFLOW_HAS_DROGON
#include <drogon/WebSocketController.h>

#include <map>
#include <mutex>

namespace taskflow::transport::websocket {

class RuntimeController final : public drogon::WebSocketController<RuntimeController, false> {
public:
  RuntimeController(Gateway &gateway, infrastructure::NotificationDelivery &delivery,
                    infrastructure::ProjectRepository &projects, std::size_t replay_batch_size);

  WS_PATH_LIST_BEGIN
  WS_PATH_ADD("/api/v1/ws");
  WS_PATH_LIST_END

  void handleNewMessage(const drogon::WebSocketConnectionPtr &connection, std::string &&message,
                        const drogon::WebSocketMessageType &type) override;
  void handleNewConnection(const drogon::HttpRequestPtr &request,
                           const drogon::WebSocketConnectionPtr &connection) override;
  void handleConnectionClosed(const drogon::WebSocketConnectionPtr &connection) override;
  void poll();
  void shutdown();

private:
  struct State {
    domain::Uuid gateway_id;
    application::AuthenticatedPrincipal principal;
    std::uint64_t highest_delivered{0};
  };

  void replay(const drogon::WebSocketConnectionPtr &connection, State &state,
              std::uint64_t after_sequence);

  Gateway *gateway_;
  infrastructure::NotificationDelivery *delivery_;
  infrastructure::ProjectRepository *projects_;
  std::size_t replay_batch_size_;
  std::mutex mutex_;
  std::map<drogon::WebSocketConnectionPtr, State, std::owner_less<drogon::WebSocketConnectionPtr>>
      connections_;
};

} // namespace taskflow::transport::websocket
#endif
