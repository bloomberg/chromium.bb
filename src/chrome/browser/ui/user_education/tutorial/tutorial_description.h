// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_DESCRIPTION_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_DESCRIPTION_H_

#include <string>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/user_education/help_bubble_params.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"

// Holds the data required to properly store histograms for a given tutorial.
// Abstract base class because best practice is to statically declare
// histograms and so we need some compile-time polymorphism to actually
// implement the RecordXXX() calls.
//
// Use MakeTutorialHistograms() below to create a concrete instance of this
// class.
class TutorialHistograms {
 public:
  TutorialHistograms() = default;
  TutorialHistograms(const TutorialHistograms& other) = delete;
  virtual ~TutorialHistograms() = default;
  void operator=(const TutorialHistograms& other) = delete;

  // Records whether the tutorial was completed or not.
  virtual void RecordComplete(bool value) = 0;

  // Records the step on which the tutorial was aborted.
  virtual void RecordAbort(int step) = 0;

  // Records whether, when an IPH offered the tutorial, the user opted into
  // seeing the tutorial or not.
  virtual void RecordIphLinkClicked(bool value) = 0;
};

namespace internal {

constexpr char kTutorialHistogramPrefix[] = "Tutorial.";

template <const char histogram_name[]>
class TutorialHistogramsImpl : public TutorialHistograms {
 public:
  explicit TutorialHistogramsImpl(int max_steps)
      : histogram_name_(histogram_name),
        completed_name_(kTutorialHistogramPrefix + histogram_name_ +
                        ".Completion"),
        aborted_name_(kTutorialHistogramPrefix + histogram_name_ +
                      ".AbortStep"),
        link_clicked_name_(kTutorialHistogramPrefix + histogram_name_ +
                           ".IPHLinkClicked"),
        max_steps_(max_steps) {}
  ~TutorialHistogramsImpl() override = default;

 protected:
  void RecordComplete(bool value) override {
    UMA_HISTOGRAM_BOOLEAN(completed_name_, value);
  }

  void RecordAbort(int step) override {
    UMA_HISTOGRAM_EXACT_LINEAR(aborted_name_, step, max_steps_);
  }

  void RecordIphLinkClicked(bool value) override {
    UMA_HISTOGRAM_BOOLEAN(link_clicked_name_, value);
  }

 private:
  const std::string histogram_name_;
  const std::string completed_name_;
  const std::string aborted_name_;
  const std::string link_clicked_name_;
  const int max_steps_;
};

}  // namespace internal

// Call to create a tutorial-specific histograms object for use with the
// tutorial. The template parameter should be a reference to a const char[]
// that is a compile-time constant. Also remember to add a matching entry to
// the "TutorialID" variant in histograms.xml corresponding to your tutorial.
//
// Example:
//   const char kMyTutorialName[] = "MyTutorial";
//   tutorial_descriptions.histograms =
//       MakeTutorialHistograms<kMyTutorialName>(
//           tutorial_description.steps.size());
template <const char* histogram_name>
std::unique_ptr<TutorialHistograms> MakeTutorialHistograms(int max_steps) {
  return std::make_unique<internal::TutorialHistogramsImpl<histogram_name>>(
      max_steps);
}

// A Struct that provides all of the data necessary to construct a Tutorial.
// A Tutorial Description is a list of Steps for a tutorial. Each step has info
// for constructing the InteractionSequence::Step from the
// TutorialDescription::Step.
struct TutorialDescription {
  using NameElementsCallback =
      base::RepeatingCallback<bool(ui::InteractionSequence*,
                                   ui::TrackedElement*)>;

  TutorialDescription();
  ~TutorialDescription();
  TutorialDescription(TutorialDescription&& other);
  TutorialDescription& operator=(TutorialDescription&& other);

  struct Step {
    Step();
    Step(absl::optional<std::u16string> title_text_,
         std::u16string body_text_,
         ui::InteractionSequence::StepType step_type_,
         ui::ElementIdentifier element_id_,
         std::string element_name_,
         HelpBubbleArrow arrow_,
         ui::CustomElementEventType event_type_ = ui::CustomElementEventType(),
         absl::optional<bool> must_remain_visible_ = absl::nullopt,
         bool transition_only_on_event_ = false,
         NameElementsCallback name_elements_callback_ = NameElementsCallback());
    Step(const Step& other);
    Step& operator=(const Step& other);
    ~Step();

    absl::optional<std::u16string> title_text;

    // The text to to populated in the bubble.
    std::u16string body_text;

    // The step type for InteractionSequence::Step.
    ui::InteractionSequence::StepType step_type;

    // The event type for the step if `step_type` is kCustomEvent.
    ui::CustomElementEventType event_type;

    // The element used by interaction sequence to observe and attach a bubble.
    ui::ElementIdentifier element_id;

    // The element, referred to by name, used by the interaction sequence
    // to observe and potentially attach a bubble. must be non-empty.
    std::string element_name;

    // The positioning of the bubble arrow.
    HelpBubbleArrow arrow = HelpBubbleArrow::kTopRight;

    // Should the element remain visible through the entire step, this should be
    // set to false for hidden steps and for shown steps that precede hidden
    // steps on the same element. if left empty the interaction sequence will
    // decide what its value should be based on the generated
    // InteractionSequence::StepBuilder
    absl::optional<bool> must_remain_visible;

    // Should the step only be completed when an event like shown or hidden only
    // happens during current step. for more information on the implementation
    // take a look at transition_only_on_event in InteractionSequence::Step
    bool transition_only_on_event = false;

    // lambda which is called on the start callback of the InteractionSequence
    // which provides the interaction sequence and the current element that
    // belongs to the step. The intention for this functionality is to name one
    // or many elements using the Framework's Specific API finding an element
    // and naming it OR using the current element from the sequence as the
    // element for naming. The return value is a boolean which controls whether
    // the Interaction Sequence should continue or not. If false is returned
    // the tutorial will abort
    NameElementsCallback name_elements_callback;

    // returns true iff all of the required parameters exist to display a
    // bubble.
    bool ShouldShowBubble() const;
  };

  // the list of TutorialDescription steps
  std::vector<Step> steps;

  // The histogram data to use. Use MakeTutorialHistograms() above to create a
  // value to use, if you want to record specific histograms for this tutorial.
  std::unique_ptr<TutorialHistograms> histograms;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_DESCRIPTION_H_
