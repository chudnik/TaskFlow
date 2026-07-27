#include "taskflow/domain/comment.hpp"

#include <cctype>

namespace taskflow::domain {

ValidationErrors validate_comment_body(const std::string_view body) {
  ValidationErrors errors;
  bool has_text = false;
  std::size_t characters = 0;
  for (const char character : body) {
    const auto byte = static_cast<unsigned char>(character);
    if ((byte & 0xC0U) != 0x80U)
      ++characters;
    if (byte >= 0x80U || std::isspace(byte) == 0)
      has_text = true;
  }
  if (!has_text)
    errors.add("body", "blank", "comment body must not be blank");
  if (characters > 10'000)
    errors.add("body", "too_long", "comment body must contain at most 10000 characters");
  return errors;
}

} // namespace taskflow::domain
