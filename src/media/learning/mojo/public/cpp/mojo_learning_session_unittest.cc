// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "media/learning/mojo/public/cpp/mojo_learning_session.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class MojoLearningSessionTest : public ::testing::Test {
 public:
  // Impl of a mojom::LearningSession that remembers call arguments.
  class FakeMojoLearningSessionImpl : public mojom::LearningSession {
   public:
    void AddExample(mojom::LearningTaskType task_type,
                    const TrainingExample& example) override {
      task_type_ = std::move(task_type);
      example_ = example;
    }

    mojom::LearningTaskType task_type_;
    TrainingExample example_;
  };

 public:
  MojoLearningSessionTest()
      : learning_session_binding_(&fake_learning_session_impl_) {}
  ~MojoLearningSessionTest() override = default;

  void SetUp() override {
    // Create a fake learner provider mojo impl.
    mojom::LearningSessionPtr learning_session_ptr;
    learning_session_binding_.Bind(mojo::MakeRequest(&learning_session_ptr));

    // Tell |learning_session_| to forward to the fake learner impl.
    learning_session_ =
        std::make_unique<MojoLearningSession>(std::move(learning_session_ptr));
  }

  // Mojo stuff.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  FakeMojoLearningSessionImpl fake_learning_session_impl_;
  mojo::Binding<mojom::LearningSession> learning_session_binding_;

  // The learner under test.
  std::unique_ptr<MojoLearningSession> learning_session_;
};

TEST_F(MojoLearningSessionTest, ExampleIsCopied) {
  TrainingExample example({FeatureValue(123), FeatureValue(456)},
                          TargetValue(1234));
  learning_session_->AddExample("unused task id", example);
  learning_session_binding_.FlushForTesting();
  EXPECT_EQ(fake_learning_session_impl_.example_, example);
}

}  // namespace learning
}  // namespace media
