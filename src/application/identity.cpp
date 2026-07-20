#include "taskflow/application/identity.hpp"

#include "taskflow/domain/identity.hpp"

#include <utility>

namespace taskflow::application {

IdentityError::IdentityError(const IdentityErrorCode code, std::string message)
    : std::runtime_error{std::move(message)}, code_{code} {}

IdentityErrorCode IdentityError::code() const noexcept { return code_; }

IdentityUseCases::IdentityUseCases(IdentityStore &store, const PasswordService &passwords)
    : store_{&store}, passwords_{&passwords} {}

domain::User IdentityUseCases::register_user(const std::string_view email,
                                             const std::string_view password) const {
  try {
    const auto normalized = domain::normalize_email(email);
    domain::validate_password(password);
    return store_->create_user(normalized, passwords_->hash_password(password));
  } catch (const domain::CredentialValidationError &error) {
    throw IdentityError{IdentityErrorCode::invalid_input, error.what()};
  }
}

domain::User IdentityUseCases::login(const std::string_view email,
                                     const std::string_view password) const {
  std::string normalized;
  try {
    normalized = domain::normalize_email(email);
  } catch (const domain::CredentialValidationError &) {
    throw IdentityError{IdentityErrorCode::invalid_credentials, "invalid credentials"};
  }
  auto credential = store_->find_credentials(normalized);
  if (!credential || !passwords_->verify_password(password, credential->password_hash)) {
    throw IdentityError{IdentityErrorCode::invalid_credentials, "invalid credentials"};
  }
  if (credential->user.status != domain::AccountStatus::active) {
    throw IdentityError{IdentityErrorCode::inactive_account, "account is inactive"};
  }
  return credential->user;
}

} // namespace taskflow::application
