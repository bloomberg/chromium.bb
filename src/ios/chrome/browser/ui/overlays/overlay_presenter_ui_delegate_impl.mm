// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_presenter_ui_delegate_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/overlays/overlay_container_coordinator.h"
#import "ios/chrome/browser/ui/overlays/overlay_coordinator_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - OverlayPresenterUIDelegateImpl::Container

OVERLAY_USER_DATA_SETUP_IMPL(OverlayPresenterUIDelegateImpl::Container);

OverlayPresenterUIDelegateImpl::Container::Container(Browser* browser)
    : browser_(browser) {
  DCHECK(browser_);
}

OverlayPresenterUIDelegateImpl::Container::~Container() = default;

OverlayPresenterUIDelegateImpl*
OverlayPresenterUIDelegateImpl::Container::UIDelegateForModality(
    OverlayModality modality) {
  auto& ui_delegate = ui_delegates_[modality];
  if (!ui_delegate) {
    ui_delegate = base::WrapUnique(
        new OverlayPresenterUIDelegateImpl(browser_, modality));
  }
  return ui_delegate.get();
}

#pragma mark - OverlayPresenterUIDelegateImpl

OverlayPresenterUIDelegateImpl::OverlayPresenterUIDelegateImpl(
    Browser* browser,
    OverlayModality modality)
    : presenter_(OverlayPresenter::FromBrowser(browser, modality)),
      shutdown_helper_(browser, presenter_),
      ui_dismissal_helper_(this),
      coordinator_factory_([OverlayRequestCoordinatorFactory
          factoryForBrowser:browser
                   modality:modality]),
      weak_factory_(this) {
  DCHECK(presenter_);
  DCHECK(coordinator_factory_);
  presenter_->SetUIDelegate(this);
}

OverlayPresenterUIDelegateImpl::~OverlayPresenterUIDelegateImpl() = default;

#pragma mark Public

void OverlayPresenterUIDelegateImpl::SetCoordinator(
    OverlayContainerCoordinator* coordinator) {
  if (coordinator_ == coordinator)
    return;
  if (coordinator_ && request_)
    DismissPresentedUI(OverlayDismissalReason::kHiding);

  coordinator_ = coordinator;

  // The new coordinator should be started before provided to the UI delegate.
  DCHECK(!coordinator_ || coordinator_.viewController);

  ShowUIForPresentedRequest();
}

#pragma mark OverlayPresenter::UIDelegate

void OverlayPresenterUIDelegateImpl::ShowOverlayUI(
    OverlayPresenter* presenter,
    OverlayRequest* request,
    OverlayDismissalCallback dismissal_callback) {
  DCHECK_EQ(presenter_, presenter);
  // Create the UI state for |request| if necessary.
  if (!GetRequestUIState(request))
    states_[request] = std::make_unique<OverlayRequestUIState>(request);
  // Present the overlay UI and update the UI state.
  GetRequestUIState(request)->OverlayPresentionRequested(
      std::move(dismissal_callback));
  SetRequest(request);
}

void OverlayPresenterUIDelegateImpl::HideOverlayUI(OverlayPresenter* presenter,
                                                   OverlayRequest* request) {
  DCHECK_EQ(presenter_, presenter);
  DCHECK_EQ(request_, request);

  OverlayRequestUIState* state = GetRequestUIState(request_);
  DCHECK(state->has_callback());

  if (coordinator_) {
    // If the request's UI is presented by the coordinator, dismiss it.  The
    // presented request will be reset when the dismissal animation finishes.
    DismissPresentedUI(OverlayDismissalReason::kHiding);
  } else {
    // Simulate dismissal if there is no container coordinator.
    state->set_dismissal_reason(OverlayDismissalReason::kHiding);
    NotifyStateOfDismissal();
  }
}

void OverlayPresenterUIDelegateImpl::CancelOverlayUI(
    OverlayPresenter* presenter,
    OverlayRequest* request) {
  DCHECK_EQ(presenter_, presenter);
  // If the coordinator is not presenting the overlay UI for |state|, it can
  // be deleted immediately.
  OverlayRequestUIState* state = GetRequestUIState(request);
  DCHECK(state);
  if (!state->has_callback() || !coordinator_) {
    states_.erase(request);
    return;
  }

  DismissPresentedUI(OverlayDismissalReason::kCancellation);
}

#pragma mark Accesors

void OverlayPresenterUIDelegateImpl::SetRequest(OverlayRequest* request) {
  if (request_ == request)
    return;
  if (request_) {
    OverlayRequestUIState* state = GetRequestUIState(request_);
    // The presented request should only be reset when the previously presented
    // request's UI has finished being dismissed.
    DCHECK(state);
    DCHECK(!state->has_callback());
    DCHECK(!state->coordinator().viewController.view.superview);
    // If the overlay was dismissed for user interaction or cancellation, then
    // the state can be destroyed, since the UI for the previously presented
    // request will never be shown again.
    OverlayDismissalReason reason = state->dismissal_reason();
    if (reason == OverlayDismissalReason::kUserInteraction ||
        reason == OverlayDismissalReason::kCancellation) {
      states_.erase(request_);
    }
  }

  request_ = request;

  // The UI state should be created before resetting the presented request.
  DCHECK(!request_ || GetRequestUIState(request_));

  ShowUIForPresentedRequest();
}

OverlayRequestUIState* OverlayPresenterUIDelegateImpl::GetRequestUIState(
    OverlayRequest* request) {
  return request ? states_[request].get() : nullptr;
}

#pragma mark Presentation and Dismissal helpers

void OverlayPresenterUIDelegateImpl::ShowUIForPresentedRequest() {
  OverlayRequestUIState* state = GetRequestUIState(request_);
  if (!state || !coordinator_)
    return;

  // Create the coordinator if necessary.
  UIViewController* container_view_controller = coordinator_.viewController;
  OverlayRequestCoordinator* overlay_coordinator = state->coordinator();
  if (!overlay_coordinator ||
      overlay_coordinator.baseViewController != container_view_controller) {
    overlay_coordinator = [coordinator_factory_
        newCoordinatorForRequest:request_
               dismissalDelegate:&ui_dismissal_helper_
              baseViewController:container_view_controller];
    state->OverlayUIWillBePresented(overlay_coordinator);
  }

  [overlay_coordinator startAnimated:!state->has_ui_been_presented()];
  state->OverlayUIWasPresented();
}

void OverlayPresenterUIDelegateImpl::DismissPresentedUI(
    OverlayDismissalReason reason) {
  OverlayRequestUIState* state = GetRequestUIState(request_);
  DCHECK(state);
  DCHECK(coordinator_);
  DCHECK(state->coordinator());

  state->set_dismissal_reason(reason);
  [state->coordinator()
      stopAnimated:reason == OverlayDismissalReason::kUserInteraction];
}

void OverlayPresenterUIDelegateImpl::OverlayUIWasDismissed() {
  DCHECK(request_);
  // Overlays are dismissed without animation when the container coordinator is
  // reset, but the state should not be notified of these dismissals since the
  // same UI will be presented again once a new container coordinator is
  // provided.
  if (!coordinator_)
    return;
  NotifyStateOfDismissal();
}

void OverlayPresenterUIDelegateImpl::NotifyStateOfDismissal() {
  DCHECK(request_);
  DCHECK(GetRequestUIState(request_)->has_callback());
  // If there is another request in the active WebState's OverlayRequestQueue,
  // executing the state's dismissal callback will trigger the presentation of
  // the next request.  If the presented request remains unchanged after calling
  // the dismissal callback, reset it to nullptr since the UI is no longer
  // presented.
  OverlayRequest* previously_presented_request = request_;
  GetRequestUIState(request_)->OverlayUIWasDismissed();
  if (request_ == previously_presented_request)
    SetRequest(nullptr);
}

#pragma mark BrowserShutdownHelper

OverlayPresenterUIDelegateImpl::BrowserShutdownHelper::BrowserShutdownHelper(
    Browser* browser,
    OverlayPresenter* presenter)
    : presenter_(presenter) {
  DCHECK(presenter_);
  browser->AddObserver(this);
}

OverlayPresenterUIDelegateImpl::BrowserShutdownHelper::
    ~BrowserShutdownHelper() = default;

void OverlayPresenterUIDelegateImpl::BrowserShutdownHelper::BrowserDestroyed(
    Browser* browser) {
  presenter_->SetUIDelegate(nullptr);
  browser->RemoveObserver(this);
}

#pragma mark OverlayDismissalHelper

OverlayPresenterUIDelegateImpl::OverlayDismissalHelper::OverlayDismissalHelper(
    OverlayPresenterUIDelegateImpl* ui_delegate)
    : ui_delegate_(ui_delegate) {
  DCHECK(ui_delegate_);
}

OverlayPresenterUIDelegateImpl::OverlayDismissalHelper::
    ~OverlayDismissalHelper() = default;

void OverlayPresenterUIDelegateImpl::OverlayDismissalHelper::
    OverlayUIDidFinishDismissal(OverlayRequest* request) {
  DCHECK(request);
  DCHECK_EQ(ui_delegate_->request_, request);
  ui_delegate_->OverlayUIWasDismissed();
}
