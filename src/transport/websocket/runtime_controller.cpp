#include "taskflow/transport/websocket/runtime_controller.hpp"

#if TASKFLOW_HAS_DROGON
#include "taskflow/transport/websocket/protocol.hpp"

#include <nlohmann/json.hpp>

namespace taskflow::transport::websocket {
namespace {

std::string event_frame(const infrastructure::NotificationEvent &event) {
  nlohmann::json frame{{"v", protocol_version},
                       {"kind", "event"},
                       {"event_id", event.event_id.to_string()},
                       {"sequence_id", event.sequence_id},
                       {"type", event.event_type},
                       {"payload", nlohmann::json::parse(event.payload)}};
  frame["project_id"] =
      event.project_id ? nlohmann::json(event.project_id->to_string()) : nlohmann::json(nullptr);
  frame["entity_id"] =
      event.entity_id ? nlohmann::json(event.entity_id->to_string()) : nlohmann::json(nullptr);
  return frame.dump();
}

} // namespace

RuntimeController::RuntimeController(Gateway &gateway,
                                     infrastructure::NotificationDelivery &delivery,
                                     infrastructure::ProjectRepository &projects,
                                     const std::size_t replay_batch_size)
    : gateway_{&gateway}, delivery_{&delivery}, projects_{&projects},
      replay_batch_size_{replay_batch_size} {}

void RuntimeController::handleNewConnection(const drogon::HttpRequestPtr &request,
                                            const drogon::WebSocketConnectionPtr &connection) {
  const auto gateway_id = gateway_->open(request->getHeader("authorization"));
  if (!gateway_id) {
    connection->shutdown(drogon::CloseCode::kViolation, "authentication required");
    return;
  }
  const auto *gateway_connection = gateway_->find(*gateway_id);
  std::lock_guard lock{mutex_};
  connections_.emplace(connection, State{*gateway_id, gateway_connection->principal, 0});
  connection->send(serialize_control("ready", 0));
}

void RuntimeController::replay(const drogon::WebSocketConnectionPtr &connection, State &state,
                               const std::uint64_t after_sequence) {
  const auto batch = delivery_->resume(state.principal.user_id, after_sequence, replay_batch_size_);
  if (batch.resync_required) {
    connection->send(serialize_control("resync_required", std::nullopt, "retention_boundary"));
    return;
  }
  for (const auto &event : batch.events) {
    if (event.project_id && !projects_->find_role(*event.project_id, state.principal.user_id)) {
      gateway_->membership_removed(state.principal.user_id, *event.project_id);
      connection->send(serialize_control("authorization_changed"));
      continue;
    }
    if (event.project_id)
      gateway_->authorize_project(state.gateway_id, *event.project_id);
    connection->send(event_frame(event));
    state.highest_delivered = event.sequence_id;
  }
}

void RuntimeController::handleNewMessage(const drogon::WebSocketConnectionPtr &connection,
                                         std::string &&message,
                                         const drogon::WebSocketMessageType &type) {
  if (type != drogon::WebSocketMessageType::Text) {
    connection->shutdown(drogon::CloseCode::kInvalidMessage, "text frames required");
    return;
  }
  const auto control = parse_client_control(message);
  if (!control) {
    connection->shutdown(drogon::CloseCode::kProtocolError, "invalid control frame");
    return;
  }
  std::lock_guard lock{mutex_};
  const auto found = connections_.find(connection);
  if (found == connections_.end())
    return;
  auto &state = found->second;
  try {
    if (control->kind == ClientControl::Kind::resume)
      replay(connection, state, *control->sequence_id);
    else if (control->kind == ClientControl::Kind::acknowledge)
      delivery_->acknowledge(state.principal.user_id, *control->sequence_id,
                             state.highest_delivered);
    else {
      gateway_->heartbeat(state.gateway_id);
      connection->send(
          nlohmann::json{{"v", protocol_version}, {"kind", "pong"}, {"nonce", *control->nonce}}
              .dump());
    }
  } catch (const std::exception &) {
    connection->send(serialize_control("error", std::nullopt, "operation_failed"));
  }
}

void RuntimeController::handleConnectionClosed(const drogon::WebSocketConnectionPtr &connection) {
  std::lock_guard lock{mutex_};
  const auto found = connections_.find(connection);
  if (found != connections_.end()) {
    gateway_->cleanup(found->second.gateway_id);
    connections_.erase(found);
  }
}

void RuntimeController::poll() {
  std::lock_guard lock{mutex_};
  gateway_->sweep();
  for (auto &[connection, state] : connections_) {
    const auto *gateway_connection = gateway_->find(state.gateway_id);
    if (gateway_connection == nullptr || gateway_connection->close_reason) {
      connection->shutdown(drogon::CloseCode::kEndpointGone, "connection expired");
      continue;
    }
    const auto subscriptions = gateway_connection->project_subscriptions;
    for (const auto &project_id : subscriptions) {
      if (!projects_->find_role(project_id, state.principal.user_id)) {
        gateway_->membership_removed(state.principal.user_id, project_id);
        connection->send(serialize_control("authorization_changed"));
        break;
      }
    }
    try {
      replay(connection, state, state.highest_delivered);
    } catch (const std::exception &) {
    }
  }
}

void RuntimeController::shutdown() {
  std::lock_guard lock{mutex_};
  for (const auto &[connection, state] : connections_) {
    static_cast<void>(state);
    connection->shutdown(drogon::CloseCode::kEndpointGone, "server shutdown");
  }
}

} // namespace taskflow::transport::websocket
#endif
