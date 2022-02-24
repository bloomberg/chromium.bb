// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/tutorial/tutorial.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/ui/user_education/help_bubble.h"
#include "chrome/browser/ui/user_education/help_bubble_factory.h"
#include "chrome/browser/ui/user_education/help_bubble_factory_registry.h"
#include "chrome/browser/ui/user_education/help_bubble_params.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_description.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"

Tutorial::StepBuilder::StepBuilder() = default;
Tutorial::StepBuilder::StepBuilder(const TutorialDescription::Step& step)
    : step_(step) {}
Tutorial::StepBuilder::~StepBuilder() = default;

// static
std::unique_ptr<ui::InteractionSequence::Step>
Tutorial::StepBuilder::BuildFromDescriptionStep(
    const TutorialDescription::Step& step,
    absl::optional<std::pair<int, int>> progress,
    bool is_last_step,
    TutorialService* tutorial_service) {
  Tutorial::StepBuilder step_builder(step);
  step_builder.SetProgress(progress).SetIsLastStep(is_last_step);

  return step_builder.Build(tutorial_service);
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetAnchorElementID(
    ui::ElementIdentifier anchor_element_id) {
  // Element ID and Element Name are mutually exclusive
  DCHECK(!anchor_element_id || step_.element_name.empty());

  step_.element_id = anchor_element_id;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetAnchorElementName(
    std::string anchor_element_name) {
  // Element ID and Element Name are mutually exclusive
  DCHECK(anchor_element_name.empty() || !step_.element_id);
  step_.element_name = anchor_element_name;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetTitleText(
    absl::optional<std::u16string> title_text_) {
  step_.title_text = title_text_;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetBodyText(
    std::u16string body_text_) {
  step_.body_text = body_text_;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetStepType(
    ui::InteractionSequence::StepType step_type_,
    ui::CustomElementEventType event_type_) {
  DCHECK_EQ(step_type_ == ui::InteractionSequence::StepType::kCustomEvent,
            static_cast<bool>(event_type_))
      << "`event_type_` should be set if and only if `step_type_` is "
         "kCustomEvent.";
  step_.step_type = step_type_;
  step_.event_type = event_type_;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetProgress(
    absl::optional<std::pair<int, int>> progress_) {
  progress = progress_;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetArrow(HelpBubbleArrow arrow_) {
  step_.arrow = arrow_;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetIsLastStep(
    bool is_last_step_) {
  is_last_step = is_last_step_;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetMustRemainVisible(
    bool must_remain_visible_) {
  step_.must_remain_visible = must_remain_visible_;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetTransitionOnlyOnEvent(
    bool transition_only_on_event_) {
  step_.transition_only_on_event = transition_only_on_event_;
  return *this;
}

Tutorial::StepBuilder& Tutorial::StepBuilder::SetNameElementsCallback(
    TutorialDescription::NameElementsCallback name_elements_callback_) {
  step_.name_elements_callback = name_elements_callback_;
  return *this;
}

std::unique_ptr<ui::InteractionSequence::Step> Tutorial::StepBuilder::Build(
    TutorialService* tutorial_service) {
  std::unique_ptr<ui::InteractionSequence::StepBuilder>
      interaction_sequence_step_builder =
          std::make_unique<ui::InteractionSequence::StepBuilder>();

  if (step_.element_id)
    interaction_sequence_step_builder->SetElementID(step_.element_id);

  if (!step_.element_name.empty())
    interaction_sequence_step_builder->SetElementName(step_.element_name);

  interaction_sequence_step_builder->SetType(step_.step_type, step_.event_type);

  if (step_.must_remain_visible.has_value())
    interaction_sequence_step_builder->SetMustRemainVisible(
        step_.must_remain_visible.value());

  interaction_sequence_step_builder->SetTransitionOnlyOnEvent(
      step_.transition_only_on_event);

  interaction_sequence_step_builder->SetStartCallback(
      BuildStartCallback(tutorial_service));
  interaction_sequence_step_builder->SetEndCallback(
      BuildHideBubbleCallback(tutorial_service));

  return interaction_sequence_step_builder->Build();
}

ui::InteractionSequence::StepStartCallback
Tutorial::StepBuilder::BuildStartCallback(TutorialService* tutorial_service) {
  // get show bubble callback
  ui::InteractionSequence::StepStartCallback maybe_show_bubble_callback =
      BuildMaybeShowBubbleCallback(tutorial_service);

  return base::BindOnce(
      [](TutorialDescription::NameElementsCallback name_elements_callback,
         ui::InteractionSequence::StepStartCallback maybe_show_bubble_callback,
         ui::InteractionSequence* sequence, ui::TrackedElement* element) {
        if (name_elements_callback)
          name_elements_callback.Run(sequence, element);
        if (maybe_show_bubble_callback)
          std::move(maybe_show_bubble_callback).Run(sequence, element);
      },
      step_.name_elements_callback, std::move(maybe_show_bubble_callback));
}

ui::InteractionSequence::StepStartCallback
Tutorial::StepBuilder::BuildMaybeShowBubbleCallback(
    TutorialService* tutorial_service) {
  if (!step_.ShouldShowBubble())
    return ui::InteractionSequence::StepStartCallback();

  return base::BindOnce(
      [](TutorialService* tutorial_service,
         absl::optional<std::u16string> title_text_, std::u16string body_text_,
         HelpBubbleArrow arrow_, absl::optional<std::pair<int, int>> progress_,
         bool is_last_step_, ui::InteractionSequence* sequence,
         ui::TrackedElement* element) {
        DCHECK(tutorial_service);

        tutorial_service->HideCurrentBubbleIfShowing();

        base::RepeatingClosure abort_callback = base::BindRepeating(
            [](TutorialService* tutorial_service) {
              tutorial_service->AbortTutorial();
            },
            base::Unretained(tutorial_service));

        HelpBubbleParams params;
        if (title_text_)
          params.title_text = *title_text_;
        params.body_text = body_text_;
        params.tutorial_progress = progress_;
        params.arrow = arrow_;
        if (!is_last_step_) {
          params.timeout = base::TimeDelta();
          params.dismiss_callback = abort_callback;
        }

        std::unique_ptr<HelpBubble> bubble =
            tutorial_service->bubble_factory_registry()->CreateHelpBubble(
                element, std::move(params));
        tutorial_service->SetCurrentBubble(std::move(bubble));
      },
      base::Unretained(tutorial_service), step_.title_text, step_.body_text,
      step_.arrow, progress, is_last_step);
}

ui::InteractionSequence::StepEndCallback
Tutorial::StepBuilder::BuildHideBubbleCallback(
    TutorialService* tutorial_service) {
  return base::BindOnce(
      [](TutorialService* tutorial_service, ui::TrackedElement* element) {},
      base::Unretained(tutorial_service));
}

Tutorial::Builder::Builder()
    : builder_(std::make_unique<ui::InteractionSequence::Builder>()) {}
Tutorial::Builder::~Builder() = default;

// static
std::unique_ptr<Tutorial> Tutorial::Builder::BuildFromDescription(
    const TutorialDescription& description,
    TutorialService* tutorial_service,
    ui::ElementContext context) {
  Tutorial::Builder builder;
  builder.SetContext(context);

  // Last step doesn't have a progress counter.
  const int max_progress =
      std::count_if(description.steps.begin(), description.steps.end(),
                    [](const auto& step) { return step.ShouldShowBubble(); }) -
      1;

  int current_step = 0;
  for (const auto& step : description.steps) {
    const bool is_last_step = &step == &description.steps.back();
    if (!is_last_step && step.ShouldShowBubble())
      ++current_step;
    const auto progress =
        !is_last_step && max_progress > 0
            ? absl::make_optional(std::make_pair(current_step, max_progress))
            : absl::nullopt;
    builder.AddStep(Tutorial::StepBuilder::BuildFromDescriptionStep(
        step, progress, is_last_step, tutorial_service));
  }
  DCHECK_EQ(current_step, max_progress);

  builder.SetAbortedCallback(base::BindOnce(
      [](TutorialService* tutorial_service, TutorialHistograms* histograms,
         ui::TrackedElement* last_element, ui::ElementIdentifier last_id,
         ui::InteractionSequence::StepType last_step_type,
         ui::InteractionSequence::AbortedReason aborted_reason) {
        tutorial_service->AbortTutorial();
        // TODO:(crbug.com/1295165) provide step number information from the
        // interaction sequence into the abort callback.
        if (histograms)
          histograms->RecordComplete(false);
        UMA_HISTOGRAM_BOOLEAN("Tutorial.Completion", false);
      },
      tutorial_service, description.histograms.get()));

  builder.SetCompletedCallback(base::BindOnce(
      [](TutorialService* tutorial_service, TutorialHistograms* histograms) {
        tutorial_service->CompleteTutorial();
        if (histograms)
          histograms->RecordComplete(true);
        UMA_HISTOGRAM_BOOLEAN("Tutorial.Completion", true);
      },
      tutorial_service, description.histograms.get()));

  return builder.Build();
}

Tutorial::Builder& Tutorial::Builder::AddStep(
    std::unique_ptr<ui::InteractionSequence::Step> step) {
  builder_->AddStep(std::move(step));
  return *this;
}

Tutorial::Builder& Tutorial::Builder::SetAbortedCallback(
    ui::InteractionSequence::AbortedCallback callback) {
  builder_->SetAbortedCallback(std::move(callback));
  return *this;
}

Tutorial::Builder& Tutorial::Builder::SetCompletedCallback(
    ui::InteractionSequence::CompletedCallback callback) {
  builder_->SetCompletedCallback(std::move(callback));
  return *this;
}

Tutorial::Builder& Tutorial::Builder::SetContext(
    ui::ElementContext element_context) {
  builder_->SetContext(element_context);
  return *this;
}

std::unique_ptr<Tutorial> Tutorial::Builder::Build() {
  return absl::WrapUnique(new Tutorial(builder_->Build()));
}

Tutorial::Tutorial(
    std::unique_ptr<ui::InteractionSequence> interaction_sequence)
    : interaction_sequence_(std::move(interaction_sequence)) {}
Tutorial::~Tutorial() = default;

void Tutorial::Start() {
  DCHECK(interaction_sequence_);
  if (interaction_sequence_)
    interaction_sequence_->Start();
}

void Tutorial::Abort() {
  if (interaction_sequence_)
    interaction_sequence_.reset();
}
