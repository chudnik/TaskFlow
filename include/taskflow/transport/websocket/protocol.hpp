#pragma once

#include "taskflow/domain/common.hpp"

#include <optional>
#include <string>

namespace taskflow::transport::websocket {

inline constexpr int protocol_version = 1;

struct ClientControl {
  enum class Kind { acknowledge, resume, ping };
  Kind kind;
  std::optional<std::uint64_t> sequence_id;
  std::optional<std::string> nonce;
};

[[nodiscard]] std::optional<ClientControl> parse_client_control(std::string_view json);
[[nodiscard]] std::string serialize_control(std::string_view kind,
                                            std::optional<std::uint64_t> sequence_id = {},
                                            std::optional<std::string_view> reason = {});

} // namespace taskflow::transport::websocket
