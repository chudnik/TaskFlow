#pragma once

#include "taskflow/application/comments.hpp"
#include "taskflow/transport/http/identity_controller.hpp"

namespace taskflow::transport::http {

class CommentController {
public:
  explicit CommentController(const application::CommentUseCases &use_cases);
  [[nodiscard]] ControllerResponse create(const application::AuthenticatedPrincipal &actor,
                                          std::string_view task_id,
                                          std::string_view json_body) const;
  [[nodiscard]] ControllerResponse list(const application::AuthenticatedPrincipal &actor,
                                        std::string_view task_id) const;
  [[nodiscard]] ControllerResponse edit(const application::AuthenticatedPrincipal &actor,
                                        std::string_view comment_id,
                                        std::string_view json_body) const;
  [[nodiscard]] ControllerResponse remove(const application::AuthenticatedPrincipal &actor,
                                          std::string_view comment_id) const;

private:
  const application::CommentUseCases *use_cases_;
};

} // namespace taskflow::transport::http
