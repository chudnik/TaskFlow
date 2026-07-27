#pragma once

#include "taskflow/transport/websocket/authentication.hpp"

#include <deque>
#include <map>
#include <set>

namespace taskflow::transport::websocket {

enum class CloseReason { token_expired, session_revoked, slow_consumer, heartbeat_timeout };

struct Connection {
  domain::Uuid id;
  application::AuthenticatedPrincipal principal;
  domain::UtcInstant last_heartbeat;
  std::deque<std::string> outbound;
  std::set<domain::Uuid> project_subscriptions;
  std::optional<CloseReason> close_reason;
};

class Gateway {
public:
  Gateway(const AuthenticationMiddleware &authentication, const domain::Clock &clock,
          std::size_t maximum_buffer, std::chrono::seconds heartbeat_timeout);
  Gateway(const AuthenticationMiddleware &authentication, const domain::Clock &clock,
          std::size_t maximum_buffer, std::chrono::seconds heartbeat_timeout,
          std::size_t maximum_connections);
  [[nodiscard]] std::optional<domain::Uuid> open(std::string_view authorization);
  [[nodiscard]] bool enqueue(const domain::Uuid &connection_id, std::string frame);
  void heartbeat(const domain::Uuid &connection_id);
  void revoke_session(const domain::Uuid &session_id);
  void authorize_project(const domain::Uuid &connection_id, const domain::Uuid &project_id);
  void membership_removed(const domain::Uuid &user_id, const domain::Uuid &project_id);
  [[nodiscard]] bool can_deliver(const domain::Uuid &connection_id,
                                 const domain::Uuid &project_id) const;
  void sweep();
  void cleanup(const domain::Uuid &connection_id);
  [[nodiscard]] const Connection *find(const domain::Uuid &connection_id) const;
  [[nodiscard]] std::size_t size() const noexcept;

private:
  const AuthenticationMiddleware *authentication_;
  const domain::Clock *clock_;
  std::size_t maximum_buffer_;
  std::chrono::seconds heartbeat_timeout_;
  std::size_t maximum_connections_;
  std::map<domain::Uuid, Connection> connections_;
};

} // namespace taskflow::transport::websocket
