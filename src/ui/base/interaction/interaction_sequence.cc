// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interaction_sequence.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_piece_forward.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"

namespace ui {

namespace {

// Runs |callback| if it is valid.
// We have a lot of callbacks that can be null, so calling through this method
// prevents accidentally trying to run a null callback.
template <typename Signature, typename... Args>
void RunIfValid(base::OnceCallback<Signature> callback, Args... args) {
  if (callback)
    std::move(callback).Run(args...);
}

// Version of AutoReset that takes a pointer-to-member and a weak reference in
// case the object that owns the value goes away before the AutoReset does.
template <class T, class U>
class SafeAutoReset {
 public:
  SafeAutoReset(base::WeakPtr<T> ptr, U T::*ref, U new_value)
      : ptr_(ptr), ref_(ref), old_value_(ptr.get()->*ref) {
    ptr.get()->*ref = new_value;
  }

  SafeAutoReset(SafeAutoReset<T, U>&& other)
      : ptr_(std::move(other.ptr_)),
        ref_(other.ref_),
        old_value_(other.old_value_) {}

  SafeAutoReset& operator=(SafeAutoReset<T, U>&& other) {
    if (this != &other) {
      Reset();
      ptr_ = std::move(other.ptr_);
      ref_ = other.ref_;
      old_value_ = other.old_value_;
    }
    return *this;
  }

  ~SafeAutoReset() { Reset(); }

 private:
  void Reset() {
    if (ptr_)
      ptr_.get()->*ref_ = old_value_;
  }

  base::WeakPtr<T> ptr_;
  U T::*ref_ = nullptr;
  U old_value_ = U();
};

// Convenience method to create a SafeAutoReset with less boilerplate.
template <class T, class U>
static SafeAutoReset<T, U> MakeSafeAutoReset(base::WeakPtr<T> ptr,
                                             U T::*ref,
                                             U new_value) {
  return SafeAutoReset<T, U>(ptr, ref, new_value);
}

// Sets step->must_remain_visible if it does not have a value.
void SetDefaultMustRemainVisibleValue(InteractionSequence::Step* step,
                                      const InteractionSequence::Step* next) {
  if (step->must_remain_visible.has_value())
    return;

  // Default for types other than kShown is false.
  if (step->type != InteractionSequence::StepType::kShown) {
    step->must_remain_visible = false;
    return;
  }

  // If the following step is to hide the same element, the default is false.
  if (next && next->type == InteractionSequence::StepType::kHidden &&
      (next->id == step->id || next->element_name == step->element_name)) {
    step->must_remain_visible = false;
    return;
  }

  // Otherwise for kShown steps, the default is true.
  step->must_remain_visible = true;
}

}  // anonymous namespace

InteractionSequence::Step::Step() = default;
InteractionSequence::Step::~Step() = default;

struct InteractionSequence::Configuration {
  Configuration() = default;
  ~Configuration() = default;

  std::list<std::unique_ptr<Step>> steps;
  ElementContext context;
  AbortedCallback aborted_callback;
  CompletedCallback completed_callback;
};

InteractionSequence::Builder::Builder()
    : configuration_(std::make_unique<Configuration>()) {}

InteractionSequence::Builder::~Builder() {
  DCHECK(!configuration_);
}

InteractionSequence::Builder& InteractionSequence::Builder::SetAbortedCallback(
    AbortedCallback callback) {
  DCHECK(!configuration_->aborted_callback);
  configuration_->aborted_callback = std::move(callback);
  return *this;
}

InteractionSequence::Builder&
InteractionSequence::Builder::SetCompletedCallback(CompletedCallback callback) {
  DCHECK(!configuration_->completed_callback);
  configuration_->completed_callback = std::move(callback);
  return *this;
}

InteractionSequence::Builder& InteractionSequence::Builder::AddStep(
    std::unique_ptr<Step> step) {
  // Do consistency checks and set up defaults.
  DCHECK(!step->id == step->uses_named_element())
      << " A step must set an identifier or a name, but not both.";
  DCHECK(configuration_->steps.empty() || !step->element)
      << " Only the initial step of a sequence may have a pre-set element.";
  DCHECK(!step->transition_only_on_event || !step->element)
      << " Pre-set element precludes transition_only_on_event.";

  // Set reasonable defaults for must_be_visible based on step type and
  // parameters.
  if (step->uses_named_element() && step->type != StepType::kHidden) {
    DCHECK(!step->must_be_visible.has_value() || step->must_be_visible.value());
    step->must_be_visible = true;
  } else {
    step->must_be_visible =
        step->must_be_visible.value_or(step->type == StepType::kActivated);
  }

  DCHECK(!step->element || step->must_be_visible.value())
      << " Initial step with associated element must be visible from start.";
  DCHECK(step->type != InteractionSequence::StepType::kHidden ||
         !step->must_remain_visible.has_value() ||
         !step->must_remain_visible.value());
  DCHECK(step->type != InteractionSequence::StepType::kShown ||
         !step->uses_named_element() || !step->transition_only_on_event)
      << " kShown steps with transition_only_on_event are not compatible with"
         " named elements since a named element ceases to be valid when it"
         " becomes hidden.";
  if (!configuration_->context)
    configuration_->context = step->context;
  else
    DCHECK(!step->context || step->context == configuration_->context);

  // Since the must_remain_visible value can be dependent on the following
  // step, we'll set it on the previous step, then set it on the final step
  // when we build the sequence.
  if (!configuration_->steps.empty()) {
    SetDefaultMustRemainVisibleValue(configuration_->steps.back().get(),
                                     step.get());
  }

  // Add the step.
  configuration_->steps.emplace_back(std::move(step));
  return *this;
}

InteractionSequence::Builder& InteractionSequence::Builder::SetContext(
    ElementContext context) {
  configuration_->context = context;
  return *this;
}

std::unique_ptr<InteractionSequence> InteractionSequence::Builder::Build() {
  DCHECK(!configuration_->steps.empty());
  // Configure defaults for the final step.
  SetDefaultMustRemainVisibleValue(configuration_->steps.back().get(), nullptr);
  DCHECK(configuration_->context)
      << "If no view is provided, Builder::SetContext() must be called.";
  return base::WrapUnique(new InteractionSequence(std::move(configuration_)));
}

InteractionSequence::StepBuilder::StepBuilder()
    : step_(std::make_unique<Step>()) {}
InteractionSequence::StepBuilder::~StepBuilder() = default;

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetElementID(ElementIdentifier element_id) {
  DCHECK(element_id);
  step_->id = element_id;
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetElementName(
    const base::StringPiece& name) {
  step_->element_name = std::string(name);
  return *this;
}

InteractionSequence::StepBuilder& InteractionSequence::StepBuilder::SetContext(
    ElementContext context) {
  DCHECK(context);
  step_->context = context;
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetMustBeVisibleAtStart(
    bool must_be_visible) {
  step_->must_be_visible = must_be_visible;
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetMustRemainVisible(
    bool must_remain_visible) {
  step_->must_remain_visible = must_remain_visible;
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetTransitionOnlyOnEvent(
    bool transition_only_on_event) {
  step_->transition_only_on_event = transition_only_on_event;
  return *this;
}

InteractionSequence::StepBuilder& InteractionSequence::StepBuilder::SetType(
    StepType step_type) {
  step_->type = step_type;
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetStartCallback(
    StepStartCallback start_callback) {
  step_->start_callback = std::move(start_callback);
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetEndCallback(StepEndCallback end_callback) {
  step_->end_callback = std::move(end_callback);
  return *this;
}

std::unique_ptr<InteractionSequence::Step>
InteractionSequence::StepBuilder::Build() {
  return std::move(step_);
}

InteractionSequence::InteractionSequence(
    std::unique_ptr<Configuration> configuration)
    : configuration_(std::move(configuration)) {
  TrackedElement* const first_element = next_step()->element;
  if (first_element) {
    DCHECK(first_element->identifier() == next_step()->id);
    DCHECK(first_element->context() == context());
    next_step()->subscription =
        ElementTracker::GetElementTracker()->AddElementHiddenCallback(
            first_element->identifier(), first_element->context(),
            base::BindRepeating(&InteractionSequence::OnElementHidden,
                                base::Unretained(this)));
  }
}

// static
std::unique_ptr<InteractionSequence::Step>
InteractionSequence::WithInitialElement(TrackedElement* element,
                                        StepStartCallback start_callback,
                                        StepEndCallback end_callback) {
  StepBuilder step;
  step.step_->element = element;
  step.SetType(StepType::kShown)
      .SetElementID(element->identifier())
      .SetContext(element->context())
      .SetMustBeVisibleAtStart(true)
      .SetMustRemainVisible(true)
      .SetStartCallback(std::move(start_callback))
      .SetEndCallback(std::move(end_callback));
  return step.Build();
}

InteractionSequence::~InteractionSequence() {
  // We can abort during a step callback, but we cannot destroy this object.
  if (started_)
    Abort(AbortedReason::kSequenceDestroyed);
}

void InteractionSequence::Start() {
  // Ensure we're not already started.
  DCHECK(!started_);
  started_ = true;
  if (missing_first_element_) {
    Abort(AbortedReason::kElementHiddenBeforeSequenceStart);
    return;
  }
  StageNextStep();
}

void InteractionSequence::RunSynchronouslyForTesting() {
  base::RunLoop run_loop;
  quit_run_loop_closure_for_testing_ = run_loop.QuitClosure();
  Start();
  run_loop.Run();
}

void InteractionSequence::NameElement(TrackedElement* element,
                                      const base::StringPiece& name) {
  DCHECK(!name.empty());
  named_elements_[std::string(name)] = SafeElementReference(element);
  DCHECK(!current_step_ || current_step_->element_name != name);
  // When possible, preload ids for named elements so we can report a more
  // correct identifier on abort.
  for (const auto& step : configuration_->steps) {
    if (step->element_name == name)
      step->id = element ? element->identifier() : ElementIdentifier();
  }

  // If this is called during a step transition, we may want to watch for
  // activation of the element for the next step. (If the next step doesn't
  // refer to the element we just named, it will already have a subscription
  // and this call will be a no-op).
  MaybeWatchForActivationDuringStepTransition();
}

TrackedElement* InteractionSequence::GetNamedElement(
    const base::StringPiece& name) {
  const auto it = named_elements_.find(std::string(name));
  TrackedElement* result = nullptr;
  if (it != named_elements_.end()) {
    result = it->second.get();
  } else {
    NOTREACHED();
  }
  return result;
}

const TrackedElement* InteractionSequence::GetNamedElement(
    const base::StringPiece& name) const {
  return const_cast<InteractionSequence*>(this)->GetNamedElement(name);
}

void InteractionSequence::OnElementShown(TrackedElement* element) {
  DCHECK_EQ(StepType::kShown, next_step()->type);
  DCHECK(element->identifier() == next_step()->id);
  // Note that we don't need to look for a named element here, as any named
  // element referenced in a kShown step must already exist, and therefore we
  // should have already transitioned or failed.
  DoStepTransition(element);
}

void InteractionSequence::OnElementActivated(TrackedElement* element) {
  DCHECK_EQ(StepType::kActivated, next_step()->type);
  DCHECK(element->identifier() == next_step()->id);
  if (MatchesNameIfSpecified(element, next_step()->element_name))
    DoStepTransition(element);
}

void InteractionSequence::OnElementHidden(TrackedElement* element) {
  if (!started_) {
    DCHECK_EQ(next_step()->element, element);
    missing_first_element_ = true;
    next_step()->subscription = ElementTracker::Subscription();
    next_step()->element = nullptr;
    return;
  }

  if (current_step_->element == element) {
    // If the current step is marked as needing to remain visible and we haven't
    // seen the triggering event for the next step, abort.
    if (current_step_->must_remain_visible.value() &&
        !activated_during_callback_) {
      Abort(AbortedReason::kElementHiddenDuringStep);
      return;
    }

    // This element pointer is no longer valid and we can stop watching.
    current_step_->subscription = ElementTracker::Subscription();
    current_step_->element = nullptr;
  }

  // If we got a hidden callback and it wasn't to abort the current step, it
  // must be because we're waiting on the next step to start.
  if (next_step() && next_step()->id == element->identifier() &&
      next_step()->type == StepType::kHidden) {
    if (next_step()->uses_named_element()) {
      // Find the named element; if it still exists, it hasn't been hidden.
      const auto it = named_elements_.find(next_step()->element_name);
      DCHECK(it != named_elements_.end());
      if (it->second.get())
        return;
    }
    DoStepTransition(element);
  }
}

void InteractionSequence::OnElementActivatedDuringStepTransition(
    TrackedElement* element) {
  if (!next_step())
    return;

  DCHECK(element->identifier() == next_step()->id);
  DCHECK(MatchesNameIfSpecified(element, next_step()->element_name));
  next_step()->element = element;
  next_step()->subscription =
      ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          next_step()->id, context(),
          base::BindRepeating(
              &InteractionSequence::OnElementHiddenDuringStepTransition,
              base::Unretained(this)));

  activated_during_callback_ = true;
}

void InteractionSequence::OnElementHiddenDuringStepTransition(
    TrackedElement* element) {
  if (!next_step() || element != next_step()->element)
    return;

  next_step()->element = nullptr;
  next_step()->subscription = ElementTracker::Subscription();
}

void InteractionSequence::OnElementHiddenWaitingForActivate(
    TrackedElement* element) {
  if (!next_step())
    return;

  if (next_step()->element == element ||
      !ElementTracker::GetElementTracker()->GetFirstMatchingElement(
          next_step()->id, context())) {
    Abort(AbortedReason::kElementNotVisibleAtStartOfStep);
  }
}

void InteractionSequence::MaybeWatchForActivationDuringStepTransition() {
  // This code only runs if we're in the middle of processing a step, there is
  // a next step of type kActivated, the step has a valid ID (which it may not
  // have if we haven't yet named the element for the next step) and we haven't
  // already added the temporary callback.
  if (!processing_step_ || configuration_->steps.empty() || !next_step()->id ||
      next_step()->type != StepType::kActivated || next_step()->subscription) {
    return;
  }

  next_step()->subscription =
      ElementTracker::GetElementTracker()->AddElementActivatedCallback(
          next_step()->id, context(),
          base::BindRepeating(
              &InteractionSequence::OnElementActivatedDuringStepTransition,
              base::Unretained((this))));
}

void InteractionSequence::DoStepTransition(TrackedElement* element) {
  // There are a number of callbacks during this method that could potentially
  // result in this InteractionSequence being destructed, so maintain a weak
  // pointer we can check to see if we need to bail out early.
  base::WeakPtr<InteractionSequence> delete_guard = weak_factory_.GetWeakPtr();
  auto* const tracker = ElementTracker::GetElementTracker();
  {
    // This block is non-re-entrant.
    DCHECK(!processing_step_);
    auto processing =
        MakeSafeAutoReset(weak_factory_.GetWeakPtr(),
                          &InteractionSequence::processing_step_, true);

    // End the current step.
    if (current_step_) {
      // Unsubscribe from any events during the step-end process. Since the step
      // has ended, conditions like "must remain visible" no longer apply.
      current_step_->subscription = ElementTracker::Subscription();
      RunIfValid(std::move(current_step_->end_callback),
                 current_step_->element.get());
      if (!delete_guard || AbortedDuringCallback())
        return;
    }

    // Set up the new current step.
    current_step_ = std::move(configuration_->steps.front());
    configuration_->steps.pop_front();
    DCHECK(!current_step_->element || current_step_->element == element);
    current_step_->element =
        current_step_->type == StepType::kHidden ? nullptr : element;
    if (current_step_->element) {
      current_step_->subscription = tracker->AddElementHiddenCallback(
          current_step_->id, context(),
          base::BindRepeating(&InteractionSequence::OnElementHidden,
                              base::Unretained(this)));
    } else {
      current_step_->subscription = ElementTracker::Subscription();
    }

    // If we've got a guard on the new current step's element having gone away
    // while we were waiting, we can release it.
    next_step_hidden_subscription_ = ElementTracker::Subscription();

    // Special care must be taken here, because theoretically *anything* could
    // happen as a result of this callback. If the next step is a shown or
    // hidden step and the element becomes shown or hidden (or it's a step that
    // requires the element to be visible and it is not), then the appropriate
    // transition (or Abort()) will happen in StageNextStep() below.
    //
    // If, however, the callback *activates* the next target element, and the
    // next element is of type kActivated, then the activation will not
    // register unless we explicitly listen for it. This will add a temporary
    // callback in the case where
    MaybeWatchForActivationDuringStepTransition();

    // Start the step. Like all callbacks, this could abort the sequence, or
    // cause `element` to become invalid. Because of this we use the element
    // field of the current step from here forward, because we've installed a
    // callback above that will null it out if it becomes invalid.
    RunIfValid(std::move(current_step_->start_callback), this,
               current_step_->element.get());
    if (!delete_guard || AbortedDuringCallback())
      return;
  }

  if (configuration_->steps.empty()) {
    // Reset anything that might cause state change during the final callback.
    // After this, Abort() will have basically no effect, since by the time it
    // gets called, both the aborted and step end callbacks will be null.
    current_step_->subscription = ElementTracker::Subscription();
    configuration_->aborted_callback.Reset();
    // Last step end callback needs to be run before sequence completed.
    // Because the InteractionSequence could conceivably be destroyed during
    // one of these callbacks, make local copies of the callbacks and data.
    base::OnceClosure quit_closure =
        std::move(quit_run_loop_closure_for_testing_);
    CompletedCallback completed_callback =
        std::move(configuration_->completed_callback);
    std::unique_ptr<Step> last_step = std::move(current_step_);
    RunIfValid(std::move(last_step->end_callback), last_step->element.get());
    RunIfValid(std::move(completed_callback));
    RunIfValid(std::move(quit_closure));
    return;
  }

  // Since we're not done, load up the next step.
  StageNextStep();
}

void InteractionSequence::StageNextStep() {
  auto* const tracker = ElementTracker::GetElementTracker();

  Step* const next = next_step();
  DCHECK(!activated_during_callback_ || next->type == StepType::kActivated);

  // Note that if the target element for the next step was activated and then
  // hidden during the previous step transition, `next_element` could be null.
  TrackedElement* next_element;
  if (activated_during_callback_ || next->element) {
    next_element = next->element;
  } else if (next->uses_named_element()) {
    next->element = GetNamedElement(next->element_name);
    next_element = next->element;
    // We should have set the ID on this step when the element was named; the
    // element may have gone away but shouldn't differ in ID from what we
    // previously recorded.
    DCHECK(!next_element || next->id == next_element->identifier());
  } else {
    next_element = tracker->GetFirstMatchingElement(next->id, context());
  }

  if (!activated_during_callback_ && next->must_be_visible.value() &&
      !next_element) {
    // We're going to abort, but we have to finish the current step first.
    if (current_step_) {
      RunIfValid(std::move(current_step_->end_callback),
                 current_step_->element.get());
    }
    // Fast forward to the next step before aborting so we get the correct
    // information on the failed step in the abort callback.
    current_step_ = std::move(configuration_->steps.front());
    configuration_->steps.pop_front();
    // We don't want to call the step-end callback during Abort() since we
    // didn't technically start the step.
    current_step_->end_callback = StepEndCallback();
    Abort(AbortedReason::kElementNotVisibleAtStartOfStep);
    return;
  }

  switch (next->type) {
    case StepType::kShown:
      if (next_element && !next->transition_only_on_event) {
        DoStepTransition(next_element);
      } else {
        DCHECK(!next->uses_named_element());
        next->subscription = tracker->AddElementShownCallback(
            next->id, context(),
            base::BindRepeating(&InteractionSequence::OnElementShown,
                                base::Unretained(this)));
      }
      break;
    case StepType::kHidden:
      if (!next_element && !next->transition_only_on_event) {
        DoStepTransition(nullptr);
      } else {
        DCHECK(next_element || !next->uses_named_element());
        next->subscription = tracker->AddElementHiddenCallback(
            next->id, context(),
            base::BindRepeating(&InteractionSequence::OnElementHidden,
                                base::Unretained(this)));
      }
      break;
    case StepType::kActivated:
      if (activated_during_callback_) {
        activated_during_callback_ = false;
        DoStepTransition(next_element);
      } else {
        DCHECK(next_element || !next->uses_named_element());
        next->subscription = tracker->AddElementActivatedCallback(
            next->id, context(),
            base::BindRepeating(&InteractionSequence::OnElementActivated,
                                base::Unretained(this)));
        // It's possible to have the element hidden between the time we stage
        // the event and when the activation would actually come in (which
        // could be never). In this case, we should abort.
        if (next_step()->must_be_visible.value()) {
          next_step_hidden_subscription_ = tracker->AddElementHiddenCallback(
              next->id, context(),
              base::BindRepeating(
                  &InteractionSequence::OnElementHiddenWaitingForActivate,
                  base::Unretained(this)));
        }
      }
      break;
  }
}

void InteractionSequence::Abort(AbortedReason reason) {
  DCHECK(started_);
  configuration_->steps.clear();
  next_step_hidden_subscription_ = ElementTracker::Subscription();
  // The current object could be destroyed during callbacks, so ensure we save
  // a handle to the testing run loop (if there is one).
  base::OnceClosure quit_closure =
      std::move(quit_run_loop_closure_for_testing_);
  if (current_step_) {
    // Stop listening for events; we don't want additional callbacks during
    // teardown.
    current_step_->subscription = ElementTracker::Subscription();
    // The current step's element could go away during a callback, so hedge our
    // bets by using a safe reference.
    SafeElementReference element(current_step_->element);
    // The entire InteractionSequence could also go away during a callback, so
    // save anything we need locally so that we don't have to access any class
    // members as we finish terminating the sequence.
    std::unique_ptr<Step> last_step = std::move(current_step_);
    AbortedCallback aborted_callback =
        std::move(configuration_->aborted_callback);
    RunIfValid(std::move(last_step->end_callback), element.get());
    RunIfValid(std::move(aborted_callback), element.get(), last_step->id,
               last_step->type, reason);
  } else {
    // Aborted before any steps were run. Pass default values.
    // Note that if the sequence has already been aborted, this is a no-op, the
    // callback will already be null.
    RunIfValid(std::move(configuration_->aborted_callback), nullptr,
               ElementIdentifier(), StepType::kShown, reason);
  }
  RunIfValid(std::move(quit_closure));
}

bool InteractionSequence::AbortedDuringCallback() const {
  // All step callbacks are sourced from the current step. If the current step
  // is null, then the sequence must have aborted (which clears out the current
  // step). Completion can only happen after step callbacks are finished
  if (current_step_)
    return false;

  DCHECK(configuration_->steps.empty());
  DCHECK(!configuration_->aborted_callback);
  return true;
}

bool InteractionSequence::MatchesNameIfSpecified(
    const TrackedElement* element,
    const base::StringPiece& name) const {
  if (name.empty())
    return true;

  const TrackedElement* const expected = GetNamedElement(name);
  DCHECK(expected);
  return element == expected;
}

InteractionSequence::Step* InteractionSequence::next_step() {
  return configuration_->steps.empty() ? nullptr
                                       : configuration_->steps.front().get();
}

ElementContext InteractionSequence::context() const {
  return configuration_->context;
}

}  // namespace ui
