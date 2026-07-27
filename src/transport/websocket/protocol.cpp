#include "taskflow/transport/websocket/protocol.hpp"

#include <nlohmann/json.hpp>

namespace taskflow::transport::websocket {

std::optional<ClientControl> parse_client_control(const std::string_view value) {
  try {
    const auto body = nlohmann::json::parse(value);
    if (!body.is_object() || body.value("v", 0) != protocol_version || !body.contains("kind") ||
        !body["kind"].is_string())
      return std::nullopt;
    const auto kind = body["kind"].get<std::string>();
    if (kind == "ack" && body.contains("sequence_id") && body["sequence_id"].is_number_unsigned())
      return ClientControl{
          ClientControl::Kind::acknowledge, body["sequence_id"].get<std::uint64_t>(), {}};
    if (kind == "resume" && body.contains("after_sequence_id") &&
        body["after_sequence_id"].is_number_unsigned())
      return ClientControl{
          ClientControl::Kind::resume, body["after_sequence_id"].get<std::uint64_t>(), {}};
    if (kind == "ping" && body.contains("nonce") && body["nonce"].is_string())
      return ClientControl{ClientControl::Kind::ping, {}, body["nonce"].get<std::string>()};
  } catch (const nlohmann::json::exception &) {
  }
  return std::nullopt;
}

std::string serialize_control(const std::string_view kind,
                              const std::optional<std::uint64_t> sequence_id,
                              const std::optional<std::string_view> reason) {
  nlohmann::json body{{"v", protocol_version}, {"kind", kind}};
  if (sequence_id)
    body["sequence_id"] = *sequence_id;
  if (reason)
    body["reason"] = *reason;
  return body.dump();
}

} // namespace taskflow::transport::websocket
