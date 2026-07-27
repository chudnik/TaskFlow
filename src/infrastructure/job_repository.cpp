#include "taskflow/infrastructure/job_repository.hpp"

namespace taskflow::infrastructure {
namespace {

const std::string &required(const QueryResult &result, const std::size_t row,
                            const std::size_t column) {
  const auto &value = result.value(row, column);
  if (!value)
    throw RepositoryError{RepositoryErrorCode::unexpected, "job row contains null"};
  return *value;
}

domain::Uuid uuid(const std::string &value) {
  const auto parsed = domain::Uuid::parse(value);
  if (!parsed)
    throw RepositoryError{RepositoryErrorCode::unexpected, "job UUID is invalid"};
  return *parsed;
}

} // namespace

JobRepository::JobRepository(PostgresConnection &connection) : connection_{&connection} {}

domain::Uuid JobRepository::schedule(std::string business_key, std::string type,
                                     std::string payload, const domain::UtcInstant scheduled_at,
                                     const std::size_t maximum_attempts,
                                     std::string correlation_id) {
  const auto id = domain::Uuid::generate();
  const auto result = connection_->execute(
      "INSERT INTO jobs(id,business_key,job_type,payload,scheduled_at,max_attempts,"
      "correlation_id) VALUES($1::uuid,$2,$3,$4::jsonb,$5::timestamptz,$6::int,$7) "
      "ON CONFLICT(business_key) DO UPDATE SET payload=EXCLUDED.payload, "
      "scheduled_at=EXCLUDED.scheduled_at, max_attempts=EXCLUDED.max_attempts, "
      "correlation_id=EXCLUDED.correlation_id, status='pending', attempt=0, "
      "lease_owner=NULL, lease_expires_at=NULL, completed_at=NULL, last_error=NULL "
      "RETURNING id::text",
      {id.to_string(), std::move(business_key), std::move(type), std::move(payload),
       domain::format_utc(scheduled_at), std::to_string(maximum_attempts),
       std::move(correlation_id)});
  return uuid(required(result, 0, 0));
}

std::vector<Job> JobRepository::lease_due(std::string worker_id, const std::size_t batch_size,
                                          const std::chrono::seconds lease_duration) {
  auto transaction = connection_->transaction();
  const auto result = transaction.execute(
      "WITH candidates AS (SELECT id FROM jobs WHERE "
      "(status='pending' AND scheduled_at <= clock_timestamp()) OR "
      "(status='running' AND lease_expires_at < clock_timestamp()) "
      "ORDER BY scheduled_at,id FOR UPDATE SKIP LOCKED LIMIT $1::bigint) "
      "UPDATE jobs j SET status='running', lease_owner=$2, "
      "lease_expires_at=clock_timestamp()+($3::bigint*interval '1 second'), "
      "attempt=attempt+1, updated_at=clock_timestamp() FROM candidates c "
      "WHERE j.id=c.id RETURNING j.id::text,j.business_key,j.job_type,j.payload::text,"
      "j.attempt::text,j.max_attempts::text,j.correlation_id",
      {std::to_string(batch_size), worker_id, std::to_string(lease_duration.count())});
  transaction.commit();
  std::vector<Job> jobs;
  jobs.reserve(result.row_count());
  for (std::size_t row = 0; row < result.row_count(); ++row)
    jobs.push_back({uuid(required(result, row, 0)), required(result, row, 1),
                    required(result, row, 2), required(result, row, 3),
                    std::stoull(required(result, row, 4)), std::stoull(required(result, row, 5)),
                    required(result, row, 6)});
  return jobs;
}

void JobRepository::succeed(const domain::Uuid &job_id, const std::string_view worker_id) {
  static_cast<void>(connection_->execute(
      "UPDATE jobs SET status='succeeded',completed_at=clock_timestamp(),lease_owner=NULL,"
      "lease_expires_at=NULL,updated_at=clock_timestamp() WHERE id=$1::uuid "
      "AND status='running' AND lease_owner=$2",
      {job_id.to_string(), std::string{worker_id}}));
}

void JobRepository::fail(const domain::Uuid &job_id, const std::string_view worker_id,
                         std::string error, const std::chrono::seconds backoff) {
  static_cast<void>(connection_->execute(
      "UPDATE jobs SET status=CASE WHEN attempt>=max_attempts THEN 'failed' ELSE 'pending' END,"
      "scheduled_at=CASE WHEN attempt>=max_attempts THEN scheduled_at ELSE "
      "clock_timestamp()+($4::bigint*interval '1 second') END,"
      "completed_at=CASE WHEN attempt>=max_attempts THEN clock_timestamp() ELSE NULL END,"
      "last_error=$3,lease_owner=NULL,lease_expires_at=NULL,updated_at=clock_timestamp() "
      "WHERE id=$1::uuid AND status='running' AND lease_owner=$2",
      {job_id.to_string(), std::string{worker_id}, std::move(error),
       std::to_string(backoff.count())}));
}

void JobRepository::cancel_business_key(const std::string_view business_key) {
  static_cast<void>(connection_->execute(
      "UPDATE jobs SET status='cancelled',completed_at=clock_timestamp(),lease_owner=NULL,"
      "lease_expires_at=NULL,updated_at=clock_timestamp() WHERE business_key=$1 "
      "AND status IN ('pending','running')",
      {std::string{business_key}}));
}

std::vector<application::LeasedJob>
JobRepository::lease_jobs(std::string worker_id, const std::size_t batch_size,
                          const std::chrono::seconds lease_duration) {
  const auto leased = lease_due(std::move(worker_id), batch_size, lease_duration);
  std::vector<application::LeasedJob> result;
  result.reserve(leased.size());
  for (const auto &job : leased)
    result.push_back({job.id, job.business_key, job.type, job.payload, job.attempt,
                      job.maximum_attempts, job.correlation_id});
  return result;
}

void JobRepository::complete_job(const domain::Uuid &job_id, const std::string_view worker_id) {
  succeed(job_id, worker_id);
}

void JobRepository::retry_job(const domain::Uuid &job_id, const std::string_view worker_id,
                              std::string error, const std::chrono::seconds backoff) {
  fail(job_id, worker_id, std::move(error), backoff);
}

void JobRepository::upsert_reminder(std::string business_key, std::string type, std::string payload,
                                    const domain::UtcInstant scheduled_at,
                                    std::string correlation_id) {
  static_cast<void>(schedule(std::move(business_key), std::move(type), std::move(payload),
                             scheduled_at, 5, std::move(correlation_id)));
}

void JobRepository::cancel_reminder(const std::string_view business_key) {
  cancel_business_key(business_key);
}

} // namespace taskflow::infrastructure
