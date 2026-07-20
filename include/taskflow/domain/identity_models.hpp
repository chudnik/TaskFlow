#pragma once

#include "taskflow/domain/common.hpp"

#include <string>

namespace taskflow::domain {

enum class GlobalRole { user, admin };
enum class AccountStatus { active, inactive };

struct User {
  Uuid id;
  std::string email;
  GlobalRole global_role;
  AccountStatus status;
  UtcInstant created_at;
  UtcInstant updated_at;
};

struct Session {
  Uuid id;
  Uuid user_id;
  Uuid token_family_id;
  UtcInstant expires_at;
  UtcInstant created_at;
  UtcInstant last_rotated_at;
  bool revoked;
};

} // namespace taskflow::domain
