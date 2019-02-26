// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_MOJO_PUBLIC_CPP_MOJO_LEARNING_SESSION_H_
#define MEDIA_LEARNING_MOJO_PUBLIC_CPP_MOJO_LEARNING_SESSION_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "media/learning/common/learning_session.h"
#include "media/learning/mojo/public/mojom/learning_session.mojom.h"

namespace media {
namespace learning {

// LearningSession implementation to forward to a remote impl via mojo.
class COMPONENT_EXPORT(MEDIA_LEARNING_MOJO) MojoLearningSession
    : public LearningSession {
 public:
  explicit MojoLearningSession(mojom::LearningSessionPtr session_ptr);
  ~MojoLearningSession() override;

  // LearningSession
  void AddExample(const std::string& task_name,
                  const TrainingExample& example) override;

 private:
  mojom::LearningSessionPtr session_ptr_;

  DISALLOW_COPY_AND_ASSIGN(MojoLearningSession);
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_MOJO_PUBLIC_CPP_MOJO_LEARNING_SESSION_H_
