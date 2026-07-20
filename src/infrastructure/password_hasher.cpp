#include "taskflow/infrastructure/password_hasher.hpp"

#include "taskflow/domain/identity.hpp"

#include <array>
#include <random>
#include <stdexcept>
#include <vector>

#if TASKFLOW_HAS_ARGON2
#include <argon2.h>
#endif

namespace taskflow::infrastructure {

PasswordHasher::PasswordHasher(const Argon2idParameters parameters) : parameters_{parameters} {
  if (parameters_.time_cost == 0 || parameters_.memory_cost_kib < 8 ||
      parameters_.parallelism == 0 || parameters_.salt_bytes < 8 || parameters_.hash_bytes < 16) {
    throw std::invalid_argument{"invalid Argon2id parameters"};
  }
}

std::string PasswordHasher::hash(const std::string_view password) const {
  domain::validate_password(password);
#if TASKFLOW_HAS_ARGON2
  std::vector<std::uint8_t> salt(parameters_.salt_bytes);
  std::random_device random;
  for (auto &byte : salt) {
    byte = static_cast<std::uint8_t>(random());
  }
  const auto encoded_length = argon2_encodedlen(
      parameters_.time_cost, parameters_.memory_cost_kib, parameters_.parallelism,
      parameters_.salt_bytes, parameters_.hash_bytes, Argon2_id);
  std::string encoded(encoded_length, '\0');
  const auto result = argon2id_hash_encoded(
      parameters_.time_cost, parameters_.memory_cost_kib, parameters_.parallelism,
      password.data(), password.size(), salt.data(), salt.size(), parameters_.hash_bytes,
      encoded.data(), encoded.size());
  if (result != ARGON2_OK) {
    throw std::runtime_error{"Argon2id password hashing failed"};
  }
  encoded.resize(encoded.find('\0'));
  return encoded;
#else
  throw std::runtime_error{"Argon2id support is disabled in this build"};
#endif
}

bool PasswordHasher::verify(const std::string_view password,
                            const std::string_view encoded_hash) const noexcept {
#if TASKFLOW_HAS_ARGON2
  const std::string terminated_hash{encoded_hash};
  return argon2id_verify(terminated_hash.c_str(), password.data(), password.size()) == ARGON2_OK;
#else
  (void)password;
  (void)encoded_hash;
  return false;
#endif
}

std::string PasswordHasher::hash_password(const std::string_view password) const {
  return hash(password);
}

bool PasswordHasher::verify_password(const std::string_view password,
                                     const std::string_view encoded_hash) const noexcept {
  return verify(password, encoded_hash);
}

} // namespace taskflow::infrastructure
