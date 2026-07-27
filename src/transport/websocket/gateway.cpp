#include "taskflow/transport/websocket/gateway.hpp"

#include <limits>

namespace taskflow::transport::websocket {

Gateway::Gateway(const AuthenticationMiddleware &authentication, const domain::Clock &clock,
                 const std::size_t maximum_buffer, const std::chrono::seconds heartbeat_timeout)
    : Gateway(authentication, clock, maximum_buffer, heartbeat_timeout,
              std::numeric_limits<std::size_t>::max()) {}

Gateway::Gateway(const AuthenticationMiddleware &authentication, const domain::Clock &clock,
                 const std::size_t maximum_buffer, const std::chrono::seconds heartbeat_timeout,
                 const std::size_t maximum_connections)
    : authentication_{&authentication}, clock_{&clock}, maximum_buffer_{maximum_buffer},
      heartbeat_timeout_{heartbeat_timeout}, maximum_connections_{maximum_connections} {}

std::optional<domain::Uuid> Gateway::open(const std::string_view authorization) {
  if (connections_.size() >= maximum_connections_)
    return std::nullopt;
  const auto principal = authentication_->authenticate_bearer(authorization);
  if (!principal || principal->expires_at <= clock_->now())
    return std::nullopt;
  const auto id = domain::Uuid::generate();
  connections_.emplace(id, Connection{id, *principal, clock_->now(), {}, {}, {}});
  return id;
}

bool Gateway::enqueue(const domain::Uuid &connection_id, std::string frame) {
  const auto found = connections_.find(connection_id);
  if (found == connections_.end() || found->second.close_reason)
    return false;
  if (found->second.outbound.size() >= maximum_buffer_) {
    found->second.close_reason = CloseReason::slow_consumer;
    return false;
  }
  found->second.outbound.push_back(std::move(frame));
  return true;
}

void Gateway::heartbeat(const domain::Uuid &connection_id) {
  const auto found = connections_.find(connection_id);
  if (found != connections_.end())
    found->second.last_heartbeat = clock_->now();
}

void Gateway::revoke_session(const domain::Uuid &session_id) {
  for (auto &[id, connection] : connections_) {
    static_cast<void>(id);
    if (connection.principal.session_id == session_id)
      connection.close_reason = CloseReason::session_revoked;
  }
}

void Gateway::authorize_project(const domain::Uuid &connection_id, const domain::Uuid &project_id) {
  const auto found = connections_.find(connection_id);
  if (found != connections_.end() && !found->second.close_reason)
    found->second.project_subscriptions.insert(project_id);
}

void Gateway::membership_removed(const domain::Uuid &user_id, const domain::Uuid &project_id) {
  for (auto &[id, connection] : connections_) {
    static_cast<void>(id);
    if (connection.principal.user_id == user_id)
      connection.project_subscriptions.erase(project_id);
  }
}

bool Gateway::can_deliver(const domain::Uuid &connection_id, const domain::Uuid &project_id) const {
  const auto found = connections_.find(connection_id);
  return found != connections_.end() && !found->second.close_reason &&
         found->second.project_subscriptions.contains(project_id);
}

void Gateway::sweep() {
  for (auto &[id, connection] : connections_) {
    static_cast<void>(id);
    if (connection.principal.expires_at <= clock_->now())
      connection.close_reason = CloseReason::token_expired;
    else if (clock_->now() - connection.last_heartbeat > heartbeat_timeout_)
      connection.close_reason = CloseReason::heartbeat_timeout;
  }
}

void Gateway::cleanup(const domain::Uuid &connection_id) { connections_.erase(connection_id); }

const Connection *Gateway::find(const domain::Uuid &connection_id) const {
  const auto found = connections_.find(connection_id);
  return found == connections_.end() ? nullptr : &found->second;
}

std::size_t Gateway::size() const noexcept { return connections_.size(); }

} // namespace taskflow::transport::websocket
