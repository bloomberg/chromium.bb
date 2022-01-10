// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_H_

#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble_factory.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble_factory_registry.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_description.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"

class TutorialService;

// Tutorials are a user initiated IPH which spans 1 or more Interactions.
// It utilizes the InteractionSequence Framework to provide a tracked list of
// interactions with tracked elements.
//
// Each tutorial consists of a list of InteractionSequence steps which, in the
// default case, create a TutorialBubble which is implementation specific to
// the platform the tutorial is written for. It is possible to create custom
// InteractionSequenceSteps when using the traditional constructor and not
// using the TutorialStepBuilder.
//
// Because of implementation details in InteractionSequence, a tutorial can only
// be run once, see documentation for InteractionSequence.
//
// Basic tutorials use a TutorialDescription struct and the
// Builder::BuildFromDescription method to construct the tutorial.
//
// the end user of a Tutorial would define a tutorial description in a
// TutorialRegistry, for the platform the tutorial is implemented on. (see
// BrowserTutorialServiceFactory)
//
// TODO: Provide an in-depth readme.md for tutorials
//
class Tutorial {
 public:
  ~Tutorial();

  // Step Builder provides an interface for constructing an
  // InteractionSequence::Step from a TutorialDescription::Step.
  // TutorialDescription is used as the basis for the StepBuilder since all
  // parameters of the Description will be needed to create the bubble or build
  // the interaction sequence step. In order to use the The StepBuilder should
  // only be used by Tutorial::Builder to construct the steps in the tutorial.
  class StepBuilder {
   public:
    StepBuilder();
    explicit StepBuilder(const TutorialDescription::Step& step);
    StepBuilder(const StepBuilder&) = delete;
    StepBuilder& operator=(const StepBuilder&) = delete;
    ~StepBuilder();

    // Constructs the InteractionSequenceStepDirectly from the
    // TutorialDescriptionStep. This method is used by
    // Tutorial::Builder::BuildFromDescription to create tutorials.
    static std::unique_ptr<ui::InteractionSequence::Step>
    BuildFromDescriptionStep(
        const TutorialDescription::Step& step,
        absl::optional<std::pair<int, int>> progress,
        bool is_last_step,
        TutorialService* tutorial_service,
        TutorialBubbleFactoryRegistry* bubble_factory_registry);

    StepBuilder& SetAnchorElementID(ui::ElementIdentifier anchor_element_id);
    StepBuilder& SetAnchorElementName(std::string anchor_element_name);
    StepBuilder& SetTitleText(absl::optional<std::u16string> title_text_);
    StepBuilder& SetBodyText(absl::optional<std::u16string> body_text_);
    StepBuilder& SetStepType(ui::InteractionSequence::StepType step_type_);
    StepBuilder& SetArrow(TutorialDescription::Step::Arrow arrow_);
    StepBuilder& SetProgress(absl::optional<std::pair<int, int>> progress_);
    StepBuilder& SetIsLastStep(bool is_last_step_);
    StepBuilder& SetMustRemainVisible(bool must_remain_visible_);
    StepBuilder& SetTransitionOnlyOnEvent(bool transition_only_on_event_);
    StepBuilder& SetNameElementsCallback(
        TutorialDescription::NameElementsCallback name_elements_callback_);

    std::unique_ptr<ui::InteractionSequence::Step> Build(
        TutorialService* tutorial_service,
        TutorialBubbleFactoryRegistry* bubble_factory_registry);

   private:
    absl::optional<std::pair<int, int>> progress;
    bool is_last_step = false;

    ui::InteractionSequence::StepStartCallback BuildStartCallback(
        TutorialService* tutorial_service,
        TutorialBubbleFactoryRegistry* bubble_factory_registry);

    ui::InteractionSequence::StepStartCallback BuildMaybeShowBubbleCallback(
        TutorialService* tutorial_service,
        TutorialBubbleFactoryRegistry* bubble_factory_registry);

    ui::InteractionSequence::StepEndCallback BuildHideBubbleCallback(
        TutorialService* tutorial_service);
    TutorialDescription::Step step_;
  };

  class Builder {
   public:
    Builder();
    ~Builder();

    static std::unique_ptr<Tutorial> BuildFromDescription(
        TutorialDescription description,
        TutorialService* tutorial_service,
        TutorialBubbleFactoryRegistry* bubble_factory_registry,
        ui::ElementContext context);

    Builder(const Builder& other) = delete;
    void operator=(Builder& other) = delete;

    Builder& AddStep(std::unique_ptr<ui::InteractionSequence::Step> step);
    Builder& SetContext(ui::ElementContext element_context);

    Builder& SetAbortedCallback(
        ui::InteractionSequence::AbortedCallback callback);
    Builder& SetCompletedCallback(
        ui::InteractionSequence::CompletedCallback callback);

    std::unique_ptr<Tutorial> Build();

   private:
    std::unique_ptr<ui::InteractionSequence::Builder> builder_;
  };

  // Starts the Tutorial. has the same constraints as
  // InteractionSequence::Start.
  void Start();

  // Cancels the Tutorial. Calls the destructor of the InteractionSequence,
  // calling the abort callback if necessary.
  void Abort();

 private:
  // Tutorial Constructor that takes an InteractionSequence. Should only be
  // used in cases where custom step logic must be called
  explicit Tutorial(
      std::unique_ptr<ui::InteractionSequence> interaction_sequence);

  // The Interaction Sequence which controls the tutorial bubbles opening and
  // closing
  std::unique_ptr<ui::InteractionSequence> interaction_sequence_;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_H_
