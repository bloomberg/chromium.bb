// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interaction_sequence.h"

#include "base/callback_forward.h"
#include "base/debug/stack_trace.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"

namespace ui {

namespace {

const char kElementName1[] = "Element1";
const char kElementName2[] = "Element2";
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier2);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestIdentifier3);
const ElementContext kTestContext1(1);
const ElementContext kTestContext2(2);

}  // namespace

TEST(InteractionSequenceTest, ConstructAndDestructContext) {
  auto sequence =
      InteractionSequence::Builder()
          .SetContext(kTestContext1)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(kTestIdentifier1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence.reset();
}

TEST(InteractionSequenceTest, ConstructAndDestructWithWithInitialElement) {
  TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto sequence =
      InteractionSequence::Builder()
          .AddStep(InteractionSequence::WithInitialElement(&element))
          .Build();
  sequence.reset();
}

TEST(InteractionSequenceTest, StartAndDestruct) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence.reset());
}

TEST(InteractionSequenceTest, StartFailsIfWithInitialElementNotVisible) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element(kTestIdentifier1, kTestContext1);
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     StartFailsIfWithInitialElementNotVisibleIdentifierOnly) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element(kTestIdentifier1, kTestContext1);
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustBeVisibleAtStart(true)
                       .SetMustRemainVisible(true)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST(InteractionSequenceTest, AbortIfWithInitialElementHiddenBeforeStart) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElementPtr element =
      std::make_unique<TestElement>(kTestIdentifier1, kTestContext1);
  element->Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(element.get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element->identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  element.reset();
  EXPECT_CALL_IN_SCOPE(
      aborted,
      Run(nullptr, ElementIdentifier(), InteractionSequence::StepType::kShown,
          InteractionSequence::AbortedReason::
              kElementHiddenBeforeSequenceStart),
      sequence->Start());
}

TEST(InteractionSequenceTest,
     AbortIfWithInitialElementHiddenBeforeStartIdentifierOnly) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElementPtr element =
      std::make_unique<TestElement>(kTestIdentifier1, kTestContext1);
  element->Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element->context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element->identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustBeVisibleAtStart(true)
                       .SetMustRemainVisible(true)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element->identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  element.reset();
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST(InteractionSequenceTest, HideWithInitialElementAborts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element.Hide());
}

TEST(InteractionSequenceTest,
     HideWithInitialElementDoesNotAbortIfMustRemainVisibleIsFalse) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustBeVisibleAtStart(true)
                       .SetMustRemainVisible(false)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetMustBeVisibleAtStart(false)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->Start();
  element.Hide();
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence.reset());
}

TEST(InteractionSequenceTest, TransitionOnActivated) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL(step, Run(sequence.get(), &element)).Times(1);
  EXPECT_CALL(completed, Run).Times(1);
  element.Activate();
}

TEST(InteractionSequenceTest, TransitionOnElementShown) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL(step, Run(sequence.get(), &element2)).Times(1);
  EXPECT_CALL(completed, Run).Times(1);
  element2.Show();
}

TEST(InteractionSequenceTest, TransitionFailsOnElementShownIfMustBeVisible) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step.Get())
                       .SetMustBeVisibleAtStart(true)
                       .Build())
          .Build();
  EXPECT_CALL(
      aborted,
      Run(nullptr, element2.identifier(), InteractionSequence::StepType::kShown,
          InteractionSequence::AbortedReason::kElementNotVisibleAtStartOfStep))
      .Times(1);
  sequence->Start();
}

TEST(InteractionSequenceTest, TransitionOnSameElementHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustRemainVisible(false)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  sequence->Start();
  element2.Show();
  EXPECT_CALL(step, Run(sequence.get(), nullptr)).Times(1);
  EXPECT_CALL(completed, Run).Times(1);
  element2.Hide();
}

TEST(InteractionSequenceTest, TransitionOnOtherElementHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL(step, Run(sequence.get(), nullptr)).Times(1);
  EXPECT_CALL(completed, Run).Times(1);
  element2.Hide();
}

TEST(InteractionSequenceTest, TransitionOnOtherElementAlreadyHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  EXPECT_CALL(step, Run(sequence.get(), testing::_)).Times(1);
  EXPECT_CALL(completed, Run).Times(1);
  sequence->Start();
}

TEST(InteractionSequenceTest, FailOnOtherElementAlreadyHiddenIfMustBeVisible) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetMustBeVisibleAtStart(true)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  EXPECT_CALL(
      aborted,
      Run(nullptr, element2.identifier(),
          InteractionSequence::StepType::kHidden,
          InteractionSequence::AbortedReason::kElementNotVisibleAtStartOfStep))
      .Times(1);
  sequence->Start();
}

TEST(InteractionSequenceTest, FailIfFirstElementBecomesHiddenBeforeActivation) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element1.Hide());
}

TEST(InteractionSequenceTest,
     FailIfSecondElementBecomesHiddenBeforeActivation) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element2.Hide());
}

TEST(InteractionSequenceTest, NoWithInitialElementTransitionsOnActivation) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetMustBeVisibleAtStart(false)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL(step, Run(sequence.get(), &element)).Times(1);
  EXPECT_CALL(completed, Run).Times(1);
  element.Activate();
}

TEST(InteractionSequenceTest, NoWithInitialElementTransitionsOnShown) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  TestElement element(kTestIdentifier1, kTestContext1);
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL(step, Run(sequence.get(), &element)).Times(1);
  EXPECT_CALL(completed, Run).Times(1);
  element.Show();
}

TEST(InteractionSequenceTest, StepEndCallbackCalled) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step_end);
  TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step_start.Get())
                       .SetEndCallback(step_end.Get())
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_3(step_start, Run(sequence.get(), &element), step_end,
                          Run(&element), completed, Run, element.Activate());
}

TEST(InteractionSequenceTest, StepEndCallbackCalledForInitialStep) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  TestElement element(kTestIdentifier1, kTestContext1);
  element.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(
              &element, step_start.Get(), step_end.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2.Get())
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(step_start, Run, sequence->Start());
  EXPECT_CALLS_IN_SCOPE_3(step_end, Run(&element), step2,
                          Run(sequence.get(), &element), completed, Run,
                          element.Activate());
}

TEST(InteractionSequenceTest, MultipleStepsComplete) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step1_start.Get())
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();

  sequence->Start();

  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());

  EXPECT_CALLS_IN_SCOPE_2(step1_end, Run, step2_start, Run,
                          element2.Activate());

  EXPECT_CALLS_IN_SCOPE_2(step2_end, Run, completed, Run, element3.Show());
}

TEST(InteractionSequenceTest, MultipleStepsWithImmediateTransition) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  element3.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step1_start.Get())
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();

  sequence->Start();

  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());

  // Since element3 is already visible, we skip straight to the end.
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(step1_end, Run).Times(1);
    EXPECT_CALL(step2_start, Run).Times(1);
    EXPECT_CALL(step2_end, Run).Times(1);
    EXPECT_CALL(completed, Run).Times(1);
  }
  element2.Activate();
}

TEST(InteractionSequenceTest, CancelMidSequenceWhenViewHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step1_start.Get())
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       // Specify that this element must remain visible:
                       .SetMustRemainVisible(true)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();

  sequence->Start();

  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());

  EXPECT_CALLS_IN_SCOPE_2(step1_end, Run, step2_start, Run,
                          element2.Activate());

  EXPECT_CALLS_IN_SCOPE_2(
      step2_end, Run, aborted,
      Run(testing::_, element2.identifier(),
          InteractionSequence::StepType::kActivated,
          InteractionSequence::AbortedReason::kElementHiddenDuringStep),
      element2.Hide());
}

TEST(InteractionSequenceTest, DontCancelIfViewDoesNotNeedToRemainVisible) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step1_start.Get())
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       // Specify that this element need not remain visible:
                       .SetMustRemainVisible(false)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();

  sequence->Start();

  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());

  EXPECT_CALLS_IN_SCOPE_2(step1_end, Run, step2_start, Run,
                          element2.Activate());

  element2.Hide();

  EXPECT_CALLS_IN_SCOPE_2(step2_end, Run, completed, Run, element3.Show());
}

TEST(InteractionSequenceTest,
     MultipleSequencesInDifferentContextsOneCompletes) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier1, kTestContext2);
  element1.Show();
  element2.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();

  auto sequence2 =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted2.Get())
          .SetCompletedCallback(completed2.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element2))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2.Get())
                       .Build())
          .Build();

  sequence->Start();
  sequence2->Start();

  EXPECT_CALLS_IN_SCOPE_2(step, Run(sequence.get(), &element1), completed, Run,
                          element1.Activate());

  EXPECT_CALL_IN_SCOPE(aborted2, Run, element2.Hide());
}

TEST(InteractionSequenceTest,
     MultipleSequencesInDifferentContextsBothComplete) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier1, kTestContext2);
  element1.Show();
  element2.Show();

  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();

  auto sequence2 =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted2.Get())
          .SetCompletedCallback(completed2.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element2))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2.Get())
                       .Build())
          .Build();

  sequence->Start();
  sequence2->Start();

  EXPECT_CALLS_IN_SCOPE_2(step, Run(sequence.get(), &element1), completed, Run,
                          element1.Activate());

  EXPECT_CALLS_IN_SCOPE_2(step2, Run(sequence2.get(), &element2), completed2,
                          Run, element2.Activate());
}

// These tests verify that events sent during callbacks (as might be used by an
// interactive UI test powered by an InteractionSequence) do not break the
// sequence.

TEST(InteractionSequenceTest, ShowDuringCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](InteractionSequence*, TrackedElement*) {
    element2.Show();
  };
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(
                           base::BindLambdaForTesting(std::move(callback)))
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step2_start.Get())
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_3(step1_end, Run, step2_start, Run, completed, Run,
                          element1.Activate());
}

TEST(InteractionSequenceTest, HideDuringCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();

  auto callback = [&](InteractionSequence*, TrackedElement*) {
    element2.Hide();
  };
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(
                           base::BindLambdaForTesting(std::move(callback)))
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetStartCallback(step2_start.Get())
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_3(step1_end, Run, step2_start, Run, completed, Run,
                          element1.Activate());
}

TEST(InteractionSequenceTest, ActivateDuringCallbackDifferentView) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();

  auto callback = [&](InteractionSequence*, TrackedElement*) {
    element2.Activate();
  };
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(
                           base::BindLambdaForTesting(std::move(callback)))
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2_start.Get())
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_3(step1_end, Run, step2_start, Run, completed, Run,
                          element1.Activate());
}

TEST(InteractionSequenceTest, ActivateDuringCallbackSameView) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](InteractionSequence*, TrackedElement*) {
    element2.Activate();
  };
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(
                           base::BindLambdaForTesting(std::move(callback)))
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2_start.Get())
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_3(step1_end, Run, step2_start, Run, completed, Run,
                          element2.Show());
}

TEST(InteractionSequenceTest, HideAfterActivateDoesntAbort) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  element3.Show();

  auto callback = [&](InteractionSequence*, TrackedElement*) {
    element2.Activate();
    element2.Hide();
  };
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(
                           base::BindLambdaForTesting(std::move(callback)))
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Show());
}

TEST(InteractionSequenceTest, HideDuringStepStartedCallbackAborts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](InteractionSequence*, TrackedElement*) {
    element2.Hide();
  };
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(
                           base::BindLambdaForTesting(std::move(callback)))
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element2.Show());
}

TEST(InteractionSequenceTest, HideDuringStepEndedCallbackAborts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](TrackedElement*) { element2.Hide(); };
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(
              &element1, InteractionSequence::StepStartCallback(),
              base::BindLambdaForTesting(std::move(callback))))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustBeVisibleAtStart(true)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST(InteractionSequenceTest, ElementHiddenDuringFinalStepStart) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](InteractionSequence*, TrackedElement*) {
    element2.Hide();
  };
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(callback))
                       .SetEndCallback(step_end.Get())
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_2(step_end, Run(nullptr), completed, Run,
                          element2.Show());
}

TEST(InteractionSequenceTest, ElementHiddenDuringFinalStepEnd) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](TrackedElement*) { element2.Hide(); };
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustRemainVisible(false)
                       .SetEndCallback(base::BindLambdaForTesting(callback))
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Show());
}

TEST(InteractionSequenceTest, ElementHiddenDuringStepEndDuringAbort) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  auto callback = [&](TrackedElement*) { element2.Hide(); };
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetEndCallback(base::BindLambdaForTesting(callback))
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();

  sequence->Start();
  element2.Show();

  // First parameter will be null because during the delete the step end
  // callback will hide the element, which happens before the abort callback is
  // called.
  EXPECT_CALL_IN_SCOPE(
      aborted,
      Run(nullptr, element2.identifier(), InteractionSequence::StepType::kShown,
          InteractionSequence::AbortedReason::kSequenceDestroyed),
      sequence.reset());
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringInitialStepStartCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  auto callback = [&](InteractionSequence*, TrackedElement*) {
    sequence.reset();
  };
  sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(
              &element1, base::BindLambdaForTesting(callback), step1_end.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();

  EXPECT_CALLS_IN_SCOPE_2(step1_end, Run, aborted, Run, sequence->Start());
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringInitialStepEndCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  auto callback = [&](TrackedElement*) { sequence.reset(); };
  sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(
              &element1, InteractionSequence::StepStartCallback(),
              base::BindLambdaForTesting(callback)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element1.Activate());
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringInitialStepAbort) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  auto callback = [&](TrackedElement*, ElementIdentifier,
                      InteractionSequence::StepType,
                      InteractionSequence::AbortedReason) { sequence.reset(); };
  sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(base::BindLambdaForTesting(callback))
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(
              &element1, step1_start.Get(), step1_end.Get()))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(step1_start, Run, sequence->Start());
  EXPECT_CALL_IN_SCOPE(step1_end, Run, element1.Hide());
  EXPECT_FALSE(sequence);
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringMidSequenceStepStart) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  auto callback = [&](InteractionSequence*, TrackedElement*) {
    sequence.reset();
  };
  sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(base::BindLambdaForTesting(callback))
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_2(step1_end, Run, aborted, Run, element2.Show());
  EXPECT_FALSE(sequence);
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringMidSequenceStepEnd) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  auto callback = [&](TrackedElement*) { sequence.reset(); };
  sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step1_start.Get())
                       .SetEndCallback(base::BindLambdaForTesting(callback))
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());
  EXPECT_CALL_IN_SCOPE(aborted, Run, element2.Activate());
  EXPECT_FALSE(sequence);
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringMidSequenceAbort) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  auto callback = [&](TrackedElement*, ElementIdentifier,
                      InteractionSequence::StepType,
                      InteractionSequence::AbortedReason) { sequence.reset(); };
  sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(base::BindLambdaForTesting(callback))
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step1_start.Get())
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());
  EXPECT_CALL_IN_SCOPE(step1_end, Run, element2.Hide());
  EXPECT_FALSE(sequence);
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringFinalStepEnd) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  auto callback = [&](TrackedElement*) { sequence.reset(); };
  sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step1_start.Get())
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(base::BindLambdaForTesting(callback))
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());
  EXPECT_CALLS_IN_SCOPE_3(step1_end, Run, step2_start, Run, completed, Run,
                          element2.Activate());
  EXPECT_FALSE(sequence);
}

TEST(InteractionSequenceTest, SequenceDestroyedDuringCompleted) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  auto callback = [&]() { sequence.reset(); };
  sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(base::BindLambdaForTesting(callback))
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step1_start.Get())
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALL_IN_SCOPE(step1_start, Run, element2.Show());
  EXPECT_CALLS_IN_SCOPE_3(step1_end, Run, step2_start, Run, step2_end, Run,
                          element2.Activate());
  EXPECT_FALSE(sequence);
}

// Test step default values:

TEST(InteractionSequenceTest, MustBeVisibleAtStart_DefaultsToTrueForActivated) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step1_end);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepEndCallback, step2_end);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  TestElement element3(kTestIdentifier3, kTestContext1);
  auto sequence =
      InteractionSequence::Builder()
          .SetContext(element1.context())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetEndCallback(step1_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();

  sequence->Start();
  EXPECT_CALLS_IN_SCOPE_3(
      step1_end, Run, step2_end, Run, aborted,
      Run(nullptr, element3.identifier(),
          InteractionSequence::StepType::kActivated,
          InteractionSequence::AbortedReason::kElementNotVisibleAtStartOfStep),
      element1.Show());
}

TEST(InteractionSequenceTest,
     MustRemainVisible_DefaultsBasedOnCurrentAndNextStep) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  TestElement element3(kTestIdentifier3, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetContext(element1.context())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          // Shown followed by hidden defaults to must_remain_visible = false.
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .Build())
          // Activated step defaults to false.
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetMustBeVisibleAtStart(false)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          // Shown followed by activated defaults to true.
          // (We will fail the sequence on this step.)
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element3.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();

  sequence->Start();
  // Trigger step 2.
  element1.Hide();
  element2.Show();
  // Trigger step 3.
  element2.Activate();
  // Trigger step 4.
  element3.Show();

  // Fail step four.
  EXPECT_CALL_IN_SCOPE(
      aborted,
      Run(&element3, element3.identifier(),
          InteractionSequence::StepType::kShown,
          InteractionSequence::AbortedReason::kElementHiddenDuringStep),
      element3.Hide());
}

// SetTransitionOnlyOnEvent tests:

TEST(InteractionSequenceTest,
     SetTransitionOnlyOnEvent_TransitionsOnDifferentElementShown) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step_start);
  // Two elements have the same identifier, but only the first is visible.
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetTransitionOnlyOnEvent(true)
                       .SetStartCallback(step_start.Get())
                       .Build())
          .Build();

  sequence->Start();

  // Fail step four.
  EXPECT_CALLS_IN_SCOPE_2(step_start, Run, completed, Run, element2.Show());
}

TEST(InteractionSequenceTest,
     SetTransitionOnlyOnEvent_TransitionsOnSameElementShown) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step_start);
  TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetContext(element1.context())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustBeVisibleAtStart(true)
                       .SetMustRemainVisible(false)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetTransitionOnlyOnEvent(true)
                       .SetStartCallback(step_start.Get())
                       .Build())
          .Build();

  sequence->Start();
  element1.Hide();

  // Fail step four.
  EXPECT_CALLS_IN_SCOPE_2(step_start, Run, completed, Run, element1.Show());
}

TEST(InteractionSequenceTest,
     SetTransitionOnlyOnEvent_TransitionsOnElementHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step_start);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kHidden)
                       .SetTransitionOnlyOnEvent(true)
                       .SetStartCallback(step_start.Get())
                       .Build())
          .Build();

  sequence->Start();
  element2.Show();

  // Fail step four.
  EXPECT_CALLS_IN_SCOPE_2(step_start, Run, completed, Run, element2.Hide());
}

// Named element tests:

TEST(InteractionSequenceTest,
     NameElement_ElementShown_NamedBeforeSequenceStarts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();
  sequence->NameElement(&element2, kElementName1);
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_ElementShown_NamedDuringStepCallback_SameIdentifier) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier1, kTestContext1);
  auto step1_start = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        EXPECT_EQ(&element1, element);
        sequence->NameElement(&element2, kElementName1);
      });
  element1.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  sequence->NameElement(&element2, kElementName1);
  EXPECT_CALLS_IN_SCOPE_2(step, Run(sequence.get(), &element2), completed, Run,
                          sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_ElementShown_NamedDuringStepCallback_DifferentIdentifiers) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  auto step1_start = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        EXPECT_EQ(&element1, element);
        sequence->NameElement(&element2, kElementName1);
      });
  element1.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step.Get())
                       .Build())
          .Build();
  sequence->NameElement(&element2, kElementName1);
  EXPECT_CALLS_IN_SCOPE_2(step, Run(sequence.get(), &element2), completed, Run,
                          sequence->Start());
}

TEST(InteractionSequenceTest, NameElement_ElementShown_FirstElementNamed) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();
  sequence->NameElement(&element1, kElementName1);
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest, NameElement_ElementShown_MultipleNamedElements) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step1_start);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::StepStartCallback, step2_start);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier1, kTestContext1);
  element1.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step1_start.Get())
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName2)
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetStartCallback(step2_start.Get())
                       .Build())
          .Build();
  sequence->NameElement(&element1, kElementName1);
  sequence->NameElement(&element2, kElementName2);
  EXPECT_CALLS_IN_SCOPE_3(step1_start, Run(sequence.get(), &element1),
                          step2_start, Run(sequence.get(), &element2),
                          completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_ElementShown_DisappearsBeforeSequenceStart) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();
  sequence->NameElement(&element1, kElementName1);
  element1.Hide();
  EXPECT_CALL_IN_SCOPE(
      aborted,
      Run(nullptr, element1.identifier(), InteractionSequence::StepType::kShown,
          InteractionSequence::AbortedReason::kElementNotVisibleAtStartOfStep),
      sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_ElementShown_DisappearsBeforeStepAbortsTheSequence) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();
  sequence->NameElement(&element2, kElementName1);
  sequence->Start();
  element2.Hide();
  EXPECT_CALL_IN_SCOPE(
      aborted,
      Run(nullptr, element2.identifier(), InteractionSequence::StepType::kShown,
          InteractionSequence::AbortedReason::kElementNotVisibleAtStartOfStep),
      element1.Activate());
}

TEST(InteractionSequenceTest,
     NameElement_ElementShown_RespectsMustRemainVisibleFalse) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .SetMustRemainVisible(false)
                       .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element1.identifier())
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->NameElement(&element2, kElementName1);
  sequence->Start();
  element2.Hide();
  EXPECT_CALL_IN_SCOPE(completed, Run, element1.Activate());
}

TEST(InteractionSequenceTest,
     NameElement_ElementActivated_NamedBeforeSequenceStarts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->NameElement(&element1, kElementName1);
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element1.Activate());
}

TEST(InteractionSequenceTest,
     NameElement_ElementActivated_NamedBeforeSequenceStarts_AbortsIfHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(element1.context())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->NameElement(&element1, kElementName1);
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element1.Hide());
}

TEST(InteractionSequenceTest, NameElement_ElementActivated_NamedDuringStep) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto step = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        sequence->NameElement(&element2, kElementName1);
      });
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1,
                                                           std::move(step)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Activate());
}

TEST(InteractionSequenceTest,
     NameElement_ElementActivated_NamedDuringStep_AbortsIfHidden) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto step = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        sequence->NameElement(&element2, kElementName1);
      });
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1,
                                                           std::move(step)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run, element2.Hide());
}

TEST(InteractionSequenceTest,
     NameElement_ElementActivated_NamedAndActivatedDuringStep) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto step = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        sequence->NameElement(&element2, kElementName1);
        element2.Activate();
      });
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1,
                                                           std::move(step)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_ElementActivated_NamedAndHiddenDuringStep_Aborts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto step = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        sequence->NameElement(&element2, kElementName1);
        element2.Hide();
      });
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1,
                                                           std::move(step)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_ElementActivated_NamedActivatedAndHiddenDuringStep) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto step = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        sequence->NameElement(&element2, kElementName1);
        element2.Activate();
        element2.Hide();
      });
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1,
                                                           std::move(step)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_ElementHidden_NamedBeforeSequenceAndHiddenBeforeSequence) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kHidden)
                       .Build())
          .Build();
  sequence->NameElement(&element2, kElementName1);
  element2.Hide();
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_ElementHidden_NamedBeforeSequenceAndHiddenDuringSequence) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kHidden)
                       .Build())
          .Build();
  sequence->NameElement(&element2, kElementName1);
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Hide());
}

TEST(InteractionSequenceTest,
     NameElement_ElementHidden_NamedDuringCallbackAndHiddenDuringSequence) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto step = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        sequence->NameElement(&element2, kElementName1);
      });
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1,
                                                           std::move(step)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kHidden)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(completed, Run, element2.Hide());
}

TEST(InteractionSequenceTest,
     NameElement_ElementHidden_NamedAndHiddenDuringCallback) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto step = base::BindLambdaForTesting(
      [&](InteractionSequence* sequence, TrackedElement* element) {
        sequence->NameElement(&element2, kElementName1);
        element2.Hide();
      });
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1,
                                                           std::move(step)))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kHidden)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest, NameElement_ElementHidden_NoElementExplicitly) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kHidden)
                       .Build())
          .Build();
  sequence->NameElement(nullptr, kElementName1);
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_ElementShown_NoElementExplicitly_Aborts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();
  sequence->NameElement(nullptr, kElementName1);
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_ElementActivated_NoElementExplicitly_Aborts) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();
  auto sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->NameElement(nullptr, kElementName1);
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST(InteractionSequenceTest,
     NameElement_TwoSequencesWithSameElementWithDifferentNames) {
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed1);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted2);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed2);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();
  element2.Show();
  auto sequence1 =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted1.Get())
          .SetCompletedCallback(completed1.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName1)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  auto sequence2 =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted2.Get())
          .SetCompletedCallback(completed2.Get())
          .AddStep(InteractionSequence::WithInitialElement(&element1))
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementName(kElementName2)
                       .SetType(InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence1->NameElement(&element2, kElementName1);
  sequence2->NameElement(&element2, kElementName2);
  sequence1->Start();
  sequence2->Start();
  EXPECT_CALL(completed1, Run).Times(1);
  EXPECT_CALL(completed2, Run).Times(1);
  element2.Activate();
}

// RunSynchronouslyForTesting() tests:

TEST(InteractionSequenceTest,
     RunSynchronouslyForTesting_SequenceAbortsDuringStart) {
  base::test::TaskEnvironment task_environment;
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner({});
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);

  std::unique_ptr<InteractionSequence> sequence;
  sequence = InteractionSequence::Builder()
                 .SetAbortedCallback(aborted.Get())
                 .SetCompletedCallback(completed.Get())
                 .SetContext(kTestContext1)
                 .AddStep(InteractionSequence::StepBuilder()
                              .SetElementID(element1.identifier())
                              .SetType(InteractionSequence::StepType::kShown)
                              .SetMustBeVisibleAtStart(true)
                              .Build())
                 .Build();
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->RunSynchronouslyForTesting());
}

TEST(InteractionSequenceTest,
     RunSynchronouslyForTesting_SequenceCompletesDuringStart) {
  base::test::TaskEnvironment task_environment;
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner({});
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  sequence = InteractionSequence::Builder()
                 .SetAbortedCallback(aborted.Get())
                 .SetCompletedCallback(completed.Get())
                 .SetContext(kTestContext1)
                 .AddStep(InteractionSequence::StepBuilder()
                              .SetElementID(element1.identifier())
                              .SetType(InteractionSequence::StepType::kShown)
                              .SetMustBeVisibleAtStart(true)
                              .Build())
                 .Build();
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

TEST(InteractionSequenceTest,
     RunSynchronouslyForTesting_SequenceAbortsDuringStep) {
  base::test::SingleThreadTaskEnvironment task_environment;
  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(
              InteractionSequence::StepBuilder()
                  .SetElementID(element1.identifier())
                  .SetType(InteractionSequence::StepType::kShown)
                  .SetMustRemainVisible(true)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](InteractionSequence*, TrackedElement*) {
                        task_environment.GetMainThreadTaskRunner()->PostTask(
                            FROM_HERE, base::BindLambdaForTesting(
                                           [&]() { element1.Hide(); }));
                      }))
                  .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->RunSynchronouslyForTesting());
}

TEST(InteractionSequenceTest,
     RunSynchronouslyForTesting_SequenceCompletesDuringStep) {
  base::test::SingleThreadTaskEnvironment task_environment;

  UNCALLED_MOCK_CALLBACK(InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(InteractionSequence::CompletedCallback, completed);
  TestElement element1(kTestIdentifier1, kTestContext1);
  TestElement element2(kTestIdentifier2, kTestContext1);
  element1.Show();

  std::unique_ptr<InteractionSequence> sequence;
  sequence =
      InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .SetContext(kTestContext1)
          .AddStep(
              InteractionSequence::StepBuilder()
                  .SetElementID(element1.identifier())
                  .SetType(InteractionSequence::StepType::kShown)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](InteractionSequence*, TrackedElement*) {
                        task_environment.GetMainThreadTaskRunner()->PostTask(
                            FROM_HERE, base::BindLambdaForTesting(
                                           [&]() { element2.Show(); }));
                      }))
                  .Build())
          .AddStep(InteractionSequence::StepBuilder()
                       .SetElementID(element2.identifier())
                       .SetType(InteractionSequence::StepType::kShown)
                       .Build())
          .Build();
  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

}  // namespace ui
