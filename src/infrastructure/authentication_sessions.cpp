#include "taskflow/infrastructure/authentication_sessions.hpp"

namespace taskflow::infrastructure {

PostgresAccountSessionValidator::PostgresAccountSessionValidator(PostgresConnection &connection)
    : connection_{&connection} {}

bool PostgresAccountSessionValidator::account_and_session_active(
    const domain::Uuid &user_id, const domain::Uuid &session_id) const {
  const auto result =
      connection_->execute("SELECT EXISTS("
                           "SELECT 1 FROM users u JOIN sessions s ON s.user_id = u.id "
                           "WHERE u.id = $1::uuid AND s.id = $2::uuid "
                           "AND u.account_status = 'active' AND s.revoked_at IS NULL "
                           "AND s.expires_at > clock_timestamp())",
                           {user_id.to_string(), session_id.to_string()});
  return result.row_count() == 1 && result.value(0, 0) == QueryParameter{"t"};
}

PostgresAuthenticationSessionStore::PostgresAuthenticationSessionStore(
    RefreshTokenService &refresh_tokens, SessionRepository &sessions, UserRepository &users)
    : refresh_tokens_{&refresh_tokens}, sessions_{&sessions}, users_{&users} {}

application::IssuedSession
PostgresAuthenticationSessionStore::create(const domain::User &user,
                                           const domain::UtcInstant expires_at) {
  auto issued = refresh_tokens_->create_session(user.id, expires_at);
  return {user, issued.session_id, std::move(issued.token), expires_at};
}

application::SessionRotation
PostgresAuthenticationSessionStore::rotate(const std::string_view refresh_token,
                                           const domain::UtcInstant expires_at) {
  auto rotated = refresh_tokens_->rotate(refresh_token, expires_at);
  if (rotated.status == RefreshRotationStatus::invalid) {
    return {application::SessionRotationStatus::invalid, std::nullopt};
  }
  if (rotated.status == RefreshRotationStatus::replay_detected || !rotated.issued) {
    return {application::SessionRotationStatus::replay_detected, std::nullopt};
  }

  auto session = sessions_->find_by_id(rotated.issued->session_id);
  if (!session) {
    return {application::SessionRotationStatus::invalid, std::nullopt};
  }
  auto user = users_->find_by_id(session->user_id);
  if (!user || user->status != domain::AccountStatus::active) {
    refresh_tokens_->logout(session->id);
    return {application::SessionRotationStatus::invalid, std::nullopt};
  }
  return {
      application::SessionRotationStatus::rotated,
      application::IssuedSession{*user, session->id, std::move(rotated.issued->token), expires_at}};
}

void PostgresAuthenticationSessionStore::logout(const domain::Uuid &session_id) {
  refresh_tokens_->logout(session_id);
}

} // namespace taskflow::infrastructure
