#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace taskflow::domain {

class CredentialValidationError : public std::invalid_argument {
public:
  using std::invalid_argument::invalid_argument;
};

[[nodiscard]] std::string normalize_email(std::string_view email);
void validate_password(std::string_view password);

} // namespace taskflow::domain
