// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interaction_sequence.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_auto_reset.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_piece.h"
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

// Insert an unused argument `Arg` in the front of the argument list for
// `callback`, and return the new callback with the dummy argument.
template <typename Arg, typename Ret, typename... Args>
base::OnceCallback<Ret(Arg, Args...)> PushUnusedArg(
    base::OnceCallback<Ret(Args...)> callback) {
  return base::BindOnce([](base::OnceCallback<Ret(Args...)> callback, Arg arg,
                           Args... args) { std::move(callback).Run(args...); },
                        std::move(callback));
}

// Insert two unused arguments `Arg1` and `Arg2` in the front of the argument
// list for `callback`, and return the new callback with the dummy arguments.
template <typename Arg1, typename Arg2, typename Ret, typename... Args>
base::OnceCallback<Ret(Arg1, Arg2, Args...)> PushUnusedArgs2(
    base::OnceCallback<Ret(Args...)> callback) {
  return base::BindOnce(
      [](base::OnceCallback<Ret(Args...)> callback, Arg1 arg1, Arg2 arg2,
         Args... args) { std::move(callback).Run(args...); },
      std::move(callback));
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
  const bool is_custom_event_any_element =
      step->type == StepType::kCustomEvent && !step->id &&
      !step->uses_named_element();
  DCHECK(is_custom_event_any_element || !step->id == step->uses_named_element())
      << " A step must set an identifier or a name, but not both.";
  DCHECK(configuration_->steps.empty() || !step->element)
      << " Only the initial step of a sequence may have a pre-set element.";
  DCHECK(!step->transition_only_on_event || !step->element)
      << " Pre-set element precludes transition_only_on_event.";
  DCHECK(!step->any_context || (!step->uses_named_element() && !step->context))
      << " Find in any context requires no context or name to be set.";
  DCHECK(!step->any_context || step->type == StepType::kShown)
      << " Currently, find in any context only supports StepType::kShown.";

  // Set reasonable defaults for must_be_visible based on step type and
  // parameters.
  if (step->uses_named_element() && step->type != StepType::kHidden) {
    DCHECK(!step->must_be_visible.has_value() || step->must_be_visible.value())
        << "Named elements not being hidden must be visible at step start.";
    step->must_be_visible = true;
  } else if (is_custom_event_any_element) {
    DCHECK(!step->must_be_visible.has_value() || !step->must_be_visible.value())
        << "A custom event with no element restrictions cannot specify that"
           " its element must start visible, as we will not know which element"
           " to refer to.";
    step->must_be_visible = false;
  } else {
    step->must_be_visible =
        step->must_be_visible.value_or(step->type == StepType::kActivated ||
                                       step->type == StepType::kCustomEvent);
  }

  DCHECK(!step->element || step->must_be_visible.value())
      << " Initial step with associated element must be visible from start.";
  DCHECK(step->type != InteractionSequence::StepType::kHidden ||
         !step->must_remain_visible.has_value() ||
         !step->must_remain_visible.value())
      << "Hide steps cannot specify that the element should remain visible.";
  DCHECK(step->type != InteractionSequence::StepType::kShown ||
         !step->uses_named_element() || !step->transition_only_on_event)
      << " kShown steps with transition_only_on_event are not compatible with"
         " named elements since a named element ceases to be valid when it"
         " becomes hidden.";
  if (!configuration_->context) {
    configuration_->context = step->context;
  } else {
    DCHECK(!step->context || step->context == configuration_->context)
        << "Cannot [currently] change context during a sequence.";
  }

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

InteractionSequence::Builder& InteractionSequence::Builder::AddStep(
    InteractionSequence::StepBuilder& step_builder) {
  return AddStep(step_builder.Build());
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

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetFindElementInAnyContext(bool any_context) {
  step_->any_context = any_context;
  return *this;
}

InteractionSequence::StepBuilder& InteractionSequence::StepBuilder::SetType(
    StepType step_type,
    CustomElementEventType event_type) {
  DCHECK_EQ(step_type == StepType::kCustomEvent, static_cast<bool>(event_type))
      << "Custom events require an event type; event type may not be specified"
         " for other step types.";
  step_->type = step_type;
  step_->custom_event_type = event_type;
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetStartCallback(
    StepStartCallback start_callback) {
  step_->start_callback = std::move(start_callback);
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetStartCallback(
    base::OnceCallback<void(TrackedElement*)> start_callback) {
  step_->start_callback =
      PushUnusedArg<InteractionSequence*>(std::move(start_callback));
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetStartCallback(
    base::OnceClosure start_callback) {
  step_->start_callback =
      PushUnusedArgs2<InteractionSequence*, TrackedElement*>(
          std::move(start_callback));
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetEndCallback(StepEndCallback end_callback) {
  step_->end_callback = std::move(end_callback);
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetEndCallback(
    base::OnceClosure end_callback) {
  step_->end_callback = PushUnusedArg<TrackedElement*>(std::move(end_callback));
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
  // activation or event on the element for the next step. (If the next step
  // doesn't refer to the element we just named, it will already have a
  // subscription and this call will be a no-op).
  MaybeWatchForTriggerDuringStepTransition();
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
  // If the element was destroyed before we got our callback, this could be
  // null.
  if (!element)
    return;
  DCHECK_EQ(StepType::kShown, next_step()->type);
  DCHECK(element->identifier() == next_step()->id);
  // Note that we don't need to look for a named element here, as any named
  // element referenced in a kShown step must already exist, and therefore we
  // should have already transitioned or failed.
  DoStepTransition(element);
}

void InteractionSequence::OnElementActivated(TrackedElement* element) {
  // If the element was destroyed before we got our callback, this could be
  // null.
  if (!element)
    return;
  DCHECK_EQ(StepType::kActivated, next_step()->type);
  DCHECK(element->identifier() == next_step()->id);
  if (MatchesNameIfSpecified(element, next_step()->element_name))
    DoStepTransition(element);
}

void InteractionSequence::OnCustomEvent(TrackedElement* element) {
  DCHECK_EQ(StepType::kCustomEvent, next_step()->type);
  if (next_step()->id && next_step()->id != element->identifier())
    return;
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
        !trigger_during_callback_) {
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

    // We can get this situation when an element goes away during a step
    // callback, before we've actually staged the following hide step. At this
    // point it's not valid to do a transition, so we'll mark that the next
    // transition has happened.
    if (processing_step_) {
      trigger_during_callback_ = true;
    } else {
      DoStepTransition(element);
    }
  }
}

void InteractionSequence::OnTriggerDuringStepTransition(
    TrackedElement* element) {
  if (!next_step() || !element)
    return;

  switch (next_step()->type) {
    case StepType::kActivated:
      // We should know the identifier and name ahead of time for activation
      // steps, so just make sure nothing has gone awry.
      DCHECK(element->identifier() == next_step()->id);
      DCHECK(MatchesNameIfSpecified(element, next_step()->element_name));
      break;
    case StepType::kCustomEvent:
      // Since we don't specify the element ID when registering for custom
      // events we have to see if we specified an ID or name and if so, whether
      // it matches the element we actually got.
      if (next_step()->id && element->identifier() != next_step()->id)
        return;
      if (!MatchesNameIfSpecified(element, next_step()->element_name))
        return;
      break;
    default:
      NOTREACHED();
      return;
  }

  // Barring disaster, we will immediately transition as soon as we finish
  // processing the current step.
  next_step()->element = element;
  trigger_during_callback_ = true;

  // Since we've hit the trigger for the next step, we need to make sure we
  // clean up (and possibly abort) if the element goes away before we can
  // finish processing the current step.
  next_step()->subscription =
      ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          element->identifier(), element->context(),
          base::BindRepeating(
              &InteractionSequence::OnElementHiddenDuringStepTransition,
              base::Unretained(this)));
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

void InteractionSequence::MaybeWatchForTriggerDuringStepTransition() {
  // This should only be called while we're processing a step, there is a next
  // step we care about, and we aren't already subscribed for an event on that
  // step.
  if (!processing_step_ || configuration_->steps.empty() ||
      next_step()->subscription) {
    return;
  }

  // If the next element is named but we have not yet named it, don't add a
  // listener; we can add one when we name the element.
  TrackedElement* named_element = nullptr;
  if (next_step()->uses_named_element()) {
    const auto it = named_elements_.find(next_step()->element_name);
    if (it != named_elements_.end())
      named_element = it->second.get();
    if (!named_element)
      return;
  }

  if (next_step()->type == StepType::kActivated && next_step()->id) {
    // If the next step is an activation step and we already know the ID (i.e.
    // it's either not a named element or the element in question has been
    // named) then add the appropriate temporary callback.
    next_step()->subscription =
        ElementTracker::GetElementTracker()->AddElementActivatedCallback(
            next_step()->id, GetElementContext(named_element),
            base::BindRepeating(
                &InteractionSequence::OnTriggerDuringStepTransition,
                base::Unretained((this))));
  } else if (next_step()->type == StepType::kCustomEvent) {
    // If the next step is a custom event, we might not yet know the name or ID
    // of the element, but we can still listen for the custom event type. We'll
    // filter out irrelevant elements in the callback itself.
    next_step()->subscription =
        ElementTracker::GetElementTracker()->AddCustomEventCallback(
            next_step()->custom_event_type, GetElementContext(named_element),
            base::BindRepeating(
                &InteractionSequence::OnTriggerDuringStepTransition,
                base::Unretained((this))));
  }
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
    base::WeakAutoReset processing(weak_factory_.GetWeakPtr(),
                                   &InteractionSequence::processing_step_,
                                   true);

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
    ++active_step_index_;
    DCHECK(!current_step_->element || current_step_->element == element);
    current_step_->element =
        current_step_->type == StepType::kHidden ? nullptr : element;
    if (current_step_->element) {
      current_step_->subscription = tracker->AddElementHiddenCallback(
          current_step_->element->identifier(),
          current_step_->element->context(),
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
    // If, however, the callback *activates* or sends a custom event on the next
    // target element, and the next step is of the matching type, then the event
    // will not register unless we explicitly listen for it. This will add a
    // temporary callback to handle this case.
    MaybeWatchForTriggerDuringStepTransition();

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
  DCHECK(!trigger_during_callback_ || next->type == StepType::kActivated ||
         next->type == StepType::kCustomEvent ||
         next->type == StepType::kHidden);

  // Note that if the target element for the next step was activated and then
  // hidden during the previous step transition, `next_element` could be null.
  TrackedElement* next_element;
  if (trigger_during_callback_ || next->element) {
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
    if (!next_element && next->any_context)
      next_element = tracker->GetElementInAnyContext(next->id);
  }

  if (!trigger_during_callback_ && next->must_be_visible.value() &&
      !next_element) {
    // We're going to abort, but we have to finish the current step first.
    if (current_step_) {
      RunIfValid(std::move(current_step_->end_callback),
                 current_step_->element.get());
    }
    Abort(AbortedReason::kElementNotVisibleAtStartOfStep);
    return;
  }

  switch (next->type) {
    case StepType::kShown:
      if (next_element && !next->transition_only_on_event) {
        DoStepTransition(next_element);
      } else {
        DCHECK(!next->uses_named_element());
        auto callback = base::BindRepeating(
            &InteractionSequence::OnElementShown, base::Unretained(this));
        next->subscription = next->any_context
                                 ? tracker->AddElementShownInAnyContextCallback(
                                       next->id, callback)
                                 : tracker->AddElementShownCallback(
                                       next->id, context(), callback);
      }
      break;
    case StepType::kHidden:
      if (trigger_during_callback_ ||
          (!next_element && !next->transition_only_on_event)) {
        trigger_during_callback_ = false;
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
      if (trigger_during_callback_) {
        trigger_during_callback_ = false;
        DoStepTransition(next_element);
      } else {
        DCHECK(next_element || !next->uses_named_element());
        const ElementContext target_context = GetElementContext(next_element);
        next->subscription = tracker->AddElementActivatedCallback(
            next->id, target_context,
            base::BindRepeating(&InteractionSequence::OnElementActivated,
                                base::Unretained(this)));
        // It's possible to have the element hidden between the time we stage
        // the event and when the activation would actually come in (which
        // could be never). In this case, we should abort.
        if (next_step()->must_be_visible.value()) {
          next_step_hidden_subscription_ = tracker->AddElementHiddenCallback(
              next->id, target_context,
              base::BindRepeating(
                  &InteractionSequence::OnElementHiddenWaitingForActivate,
                  base::Unretained(this)));
        }
      }
      break;
    case StepType::kCustomEvent:
      if (trigger_during_callback_) {
        trigger_during_callback_ = false;
        DoStepTransition(next_element);
      } else {
        DCHECK(next_element || !next->uses_named_element());
        const ElementContext target_context = GetElementContext(next_element);
        next->subscription = tracker->AddCustomEventCallback(
            next->custom_event_type, target_context,
            base::BindRepeating(&InteractionSequence::OnCustomEvent,
                                base::Unretained(this)));
        // It's possible to have the element hidden between the time we stage
        // the event and when the custom event would actually come in (which
        // could be never). In this case, we should abort.
        if (next_step()->must_be_visible.value()) {
          DCHECK(next->id);
          next_step_hidden_subscription_ = tracker->AddElementHiddenCallback(
              next->id, target_context,
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
  next_step_hidden_subscription_ = ElementTracker::Subscription();
  // The entire InteractionSequence could also go away during a callback, so
  // save anything we need locally so that we don't have to access any class
  // members as we finish terminating the sequence.
  base::OnceClosure quit_closure =
      std::move(quit_run_loop_closure_for_testing_);
  std::unique_ptr<Step> current_step = std::move(current_step_);
  AbortedCallback aborted_callback =
      std::move(configuration_->aborted_callback);
  int active_step_index = active_step_index_;
  StepType target_step_type = StepType::kShown;
  ElementIdentifier target_id;
  // The element could go away independently of the sequence.
  SafeElementReference target_element;
  if (reason == AbortedReason::kElementNotVisibleAtStartOfStep ||
      reason == AbortedReason::kElementHiddenBeforeSequenceStart) {
    ++active_step_index;
    if (next_step()) {
      target_step_type = next_step()->type;
      target_id = next_step()->id;
    }
  } else if (current_step) {
    target_step_type = current_step->type;
    target_id = current_step->id;
    target_element = SafeElementReference(current_step->element);
  }
  configuration_->steps.clear();

  // Note that if the sequence has already been aborted, this is a no-op, the
  // callbacks will already be null.
  if (current_step) {
    // Stop listening for events; we don't want additional callbacks during
    // teardown.
    current_step->subscription = ElementTracker::Subscription();
    RunIfValid(std::move(current_step->end_callback), current_step->element);
  }
  RunIfValid(std::move(aborted_callback), active_step_index,
             target_element.get(), target_id, target_step_type, reason);
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

ElementContext InteractionSequence::GetElementContext(
    const TrackedElement* element) const {
  return element ? element->context() : context();
}

}  // namespace ui
