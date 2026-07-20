#include "taskflow/domain/identity.hpp"

#include <algorithm>
#include <cctype>

namespace taskflow::domain {

std::string normalize_email(const std::string_view email) {
  const auto first = email.find_first_not_of(" \t\r\n");
  const auto last = email.find_last_not_of(" \t\r\n");
  if (first == std::string_view::npos) {
    throw CredentialValidationError{"email must not be empty"};
  }
  std::string normalized{email.substr(first, last - first + 1)};
  std::ranges::transform(normalized, normalized.begin(), [](const unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  const auto at = normalized.find('@');
  if (at == std::string::npos || at == 0 || at + 1 == normalized.size() ||
      normalized.find('@', at + 1) != std::string::npos || normalized.size() > 320) {
    throw CredentialValidationError{"email is invalid"};
  }
  return normalized;
}

void validate_password(const std::string_view password) {
  if (password.size() < 12) {
    throw CredentialValidationError{"password must contain at least 12 characters"};
  }
  if (password.size() > 1024) {
    throw CredentialValidationError{"password must not exceed 1024 bytes"};
  }
  if (password.find('\0') != std::string_view::npos) {
    throw CredentialValidationError{"password must not contain NUL bytes"};
  }
}

} // namespace taskflow::domain
