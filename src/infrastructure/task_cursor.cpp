#include "taskflow/infrastructure/task_cursor.hpp"

#include <nlohmann/json.hpp>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <array>
#include <vector>

namespace taskflow::infrastructure {
namespace {

std::string base64url(const unsigned char *data, const std::size_t size) {
  std::string encoded(4 * ((size + 2) / 3), '\0');
  const auto length = EVP_EncodeBlock(reinterpret_cast<unsigned char *>(encoded.data()), data,
                                      static_cast<int>(size));
  encoded.resize(static_cast<std::size_t>(length));
  for (auto &character : encoded) {
    if (character == '+')
      character = '-';
    if (character == '/')
      character = '_';
  }
  while (!encoded.empty() && encoded.back() == '=')
    encoded.pop_back();
  return encoded;
}

std::vector<unsigned char> decode64(std::string_view value) {
  std::string padded{value};
  for (auto &character : padded) {
    if (character == '-')
      character = '+';
    if (character == '_')
      character = '/';
  }
  while (padded.size() % 4 != 0)
    padded.push_back('=');
  std::vector<unsigned char> decoded((padded.size() / 4) * 3);
  const auto length =
      EVP_DecodeBlock(decoded.data(), reinterpret_cast<const unsigned char *>(padded.data()),
                      static_cast<int>(padded.size()));
  if (length < 0) {
    throw application::CursorError{application::CursorErrorCode::malformed,
                                   "cursor is not valid base64url"};
  }
  std::size_t actual = static_cast<std::size_t>(length);
  if (!padded.empty() && padded.back() == '=')
    --actual;
  if (padded.size() > 1 && padded[padded.size() - 2] == '=')
    --actual;
  decoded.resize(actual);
  return decoded;
}

std::array<unsigned char, 32> sign(std::string_view payload, std::string_view secret) {
  std::array<unsigned char, 32> signature{};
  unsigned int size = 0;
  HMAC(EVP_sha256(), secret.data(), static_cast<int>(secret.size()),
       reinterpret_cast<const unsigned char *>(payload.data()), payload.size(), signature.data(),
       &size);
  return signature;
}

} // namespace

SignedTaskCursorCodec::SignedTaskCursorCodec(std::string secret) : secret_{std::move(secret)} {
  if (secret_.size() < 32) {
    throw std::invalid_argument{"task cursor secret must contain at least 32 bytes"};
  }
}

std::string SignedTaskCursorCodec::encode(const application::TaskCursor &cursor) const {
  nlohmann::json document{
      {"v", cursor.version},
      {"q", cursor.query_fingerprint},
      {"s", cursor.sort_value ? nlohmann::json(*cursor.sort_value) : nlohmann::json(nullptr)},
      {"id", cursor.task_id.to_string()}};
  const auto payload = document.dump();
  const auto signature = sign(payload, secret_);
  return base64url(reinterpret_cast<const unsigned char *>(payload.data()), payload.size()) + "." +
         base64url(signature.data(), signature.size());
}

application::TaskCursor
SignedTaskCursorCodec::decode(const std::string_view encoded,
                              const std::string_view expected_fingerprint) const {
  const auto separator = encoded.find('.');
  if (separator == std::string_view::npos ||
      encoded.find('.', separator + 1) != std::string_view::npos) {
    throw application::CursorError{application::CursorErrorCode::malformed,
                                   "cursor envelope is malformed"};
  }
  const auto payload_bytes = decode64(encoded.substr(0, separator));
  const auto supplied = decode64(encoded.substr(separator + 1));
  const std::string payload{payload_bytes.begin(), payload_bytes.end()};
  const auto expected = sign(payload, secret_);
  if (supplied.size() != expected.size() ||
      CRYPTO_memcmp(supplied.data(), expected.data(), expected.size()) != 0) {
    throw application::CursorError{application::CursorErrorCode::invalid_signature,
                                   "cursor signature is invalid"};
  }
  try {
    const auto document = nlohmann::json::parse(payload);
    if (document.at("v").get<int>() != 1) {
      throw application::CursorError{application::CursorErrorCode::unsupported_version,
                                     "cursor version is unsupported"};
    }
    const auto fingerprint = document.at("q").get<std::string>();
    if (fingerprint != expected_fingerprint) {
      throw application::CursorError{application::CursorErrorCode::query_mismatch,
                                     "cursor does not match task query"};
    }
    const auto id = domain::Uuid::parse(document.at("id").get<std::string>());
    if (!id) {
      throw application::CursorError{application::CursorErrorCode::malformed,
                                     "cursor task ID is invalid"};
    }
    return {1, std::move(fingerprint),
            document.at("s").is_null() ? std::nullopt
                                       : std::optional{document.at("s").get<std::string>()},
            *id};
  } catch (const application::CursorError &) {
    throw;
  } catch (const nlohmann::json::exception &error) {
    throw application::CursorError{application::CursorErrorCode::malformed,
                                   std::string{"cursor payload is malformed: "} + error.what()};
  }
}

} // namespace taskflow::infrastructure
