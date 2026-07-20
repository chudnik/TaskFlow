#pragma once

#include "taskflow/domain/identity_models.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace taskflow::application {

enum class IdentityErrorCode { invalid_input, duplicate_email, invalid_credentials, inactive_account };

class IdentityError : public std::runtime_error {
public:
  IdentityError(IdentityErrorCode code, std::string message);
  [[nodiscard]] IdentityErrorCode code() const noexcept;

private:
  IdentityErrorCode code_;
};

struct StoredCredential {
  domain::User user;
  std::string password_hash;
};

class IdentityStore {
public:
  virtual ~IdentityStore() = default;
  [[nodiscard]] virtual domain::User create_user(std::string normalized_email,
                                                  std::string password_hash) = 0;
  [[nodiscard]] virtual std::optional<StoredCredential>
  find_credentials(std::string_view normalized_email) = 0;
};

class PasswordService {
public:
  virtual ~PasswordService() = default;
  [[nodiscard]] virtual std::string hash_password(std::string_view password) const = 0;
  [[nodiscard]] virtual bool verify_password(std::string_view password,
                                             std::string_view encoded_hash) const noexcept = 0;
};

class IdentityUseCases {
public:
  IdentityUseCases(IdentityStore &store, const PasswordService &passwords);
  [[nodiscard]] domain::User register_user(std::string_view email,
                                           std::string_view password) const;
  [[nodiscard]] domain::User login(std::string_view email, std::string_view password) const;

private:
  IdentityStore *store_;
  const PasswordService *passwords_;
};

} // namespace taskflow::application
