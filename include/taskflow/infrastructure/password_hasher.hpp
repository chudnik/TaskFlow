#pragma once

#include "taskflow/application/identity.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace taskflow::infrastructure {

struct Argon2idParameters {
  std::uint32_t time_cost{3};
  std::uint32_t memory_cost_kib{65536};
  std::uint32_t parallelism{1};
  std::uint32_t salt_bytes{16};
  std::uint32_t hash_bytes{32};
};

class PasswordHasher final : public application::PasswordService {
public:
  explicit PasswordHasher(Argon2idParameters parameters = {});

  [[nodiscard]] std::string hash(std::string_view password) const;
  [[nodiscard]] bool verify(std::string_view password, std::string_view encoded_hash) const noexcept;
  [[nodiscard]] std::string hash_password(std::string_view password) const override;
  [[nodiscard]] bool verify_password(std::string_view password,
                                     std::string_view encoded_hash) const noexcept override;

private:
  Argon2idParameters parameters_;
};

} // namespace taskflow::infrastructure
