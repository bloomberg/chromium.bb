// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_list.h"

#include <algorithm>
#include <utility>

#include "base/auto_reset.h"
#include "base/check_op.h"
#import "ios/chrome/browser/web_state_list/web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/web_state_list/web_state_list_order_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns whether the given flag is set in a flagset.
bool IsInsertionFlagSet(int flagset, WebStateList::InsertionFlags flag) {
  return (flagset & flag) == flag;
}

// Returns whether the given flag is set in a flagset.
bool IsClosingFlagSet(int flagset, WebStateList::ClosingFlags flag) {
  return (flagset & flag) == flag;
}

}  // namespace

// Wrapper around a WebState stored in a WebStateList.
class WebStateList::WebStateWrapper {
 public:
  explicit WebStateWrapper(std::unique_ptr<web::WebState> web_state);
  ~WebStateWrapper();

  web::WebState* web_state() const { return web_state_.get(); }

  // Returns ownership of the wrapped WebState.
  std::unique_ptr<web::WebState> ReleaseWebState();

  // Replaces the wrapped WebState (and clear associated state) and returns the
  // old WebState after forfeiting ownership.
  std::unique_ptr<web::WebState> ReplaceWebState(
      std::unique_ptr<web::WebState> web_state);

  // Gets and sets information about this WebState opener. The navigation index
  // is used to detect navigation changes during the same session.
  WebStateOpener opener() const { return opener_; }
  void SetOpener(WebStateOpener opener);

  // Returns whether |opener| spawned the wrapped WebState. If |use_group| is
  // true, also use the opener navigation index to detect navigation changes
  // during the same session.
  bool WasOpenedBy(const web::WebState* opener,
                   int opener_navigation_index,
                   bool use_group) const;

 private:
  std::unique_ptr<web::WebState> web_state_;
  WebStateOpener opener_;

  DISALLOW_COPY_AND_ASSIGN(WebStateWrapper);
};

WebStateList::WebStateWrapper::WebStateWrapper(
    std::unique_ptr<web::WebState> web_state)
    : web_state_(std::move(web_state)), opener_(nullptr) {
  DCHECK(web_state_);
}

WebStateList::WebStateWrapper::~WebStateWrapper() = default;

std::unique_ptr<web::WebState>
WebStateList::WebStateWrapper::ReleaseWebState() {
  std::unique_ptr<web::WebState> web_state;
  std::swap(web_state, web_state_);
  opener_ = WebStateOpener();
  return web_state;
}

std::unique_ptr<web::WebState> WebStateList::WebStateWrapper::ReplaceWebState(
    std::unique_ptr<web::WebState> web_state) {
  DCHECK_NE(web_state.get(), web_state_.get());
  DCHECK_NE(web_state.get(), nullptr);
  std::swap(web_state, web_state_);
  opener_ = WebStateOpener();
  return web_state;
}

void WebStateList::WebStateWrapper::SetOpener(WebStateOpener opener) {
  DCHECK_NE(web_state_.get(), opener.opener);
  opener_ = opener;
}

bool WebStateList::WebStateWrapper::WasOpenedBy(const web::WebState* opener,
                                                int opener_navigation_index,
                                                bool use_group) const {
  DCHECK(opener);
  if (opener_.opener != opener)
    return false;

  if (!use_group)
    return true;

  return opener_.navigation_index == opener_navigation_index;
}

WebStateList::WebStateList(WebStateListDelegate* delegate)
    : delegate_(delegate),
      order_controller_(std::make_unique<WebStateListOrderController>(this)) {
  DCHECK(delegate_);
}

WebStateList::~WebStateList() {
  CloseAllWebStates(CLOSE_NO_FLAGS);
}

bool WebStateList::ContainsIndex(int index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return 0 <= index && index < count();
}

bool WebStateList::IsMutating() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return locked_;
}

bool WebStateList::IsBatchInProgress() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return batch_operation_in_progress_;
}

web::WebState* WebStateList::GetActiveWebState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (active_index_ != kInvalidIndex)
    return GetWebStateAt(active_index_);
  return nullptr;
}

web::WebState* WebStateList::GetWebStateAt(int index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ContainsIndex(index));
  return web_state_wrappers_[index]->web_state();
}

int WebStateList::GetIndexOfWebState(const web::WebState* web_state) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (int index = 0; index < count(); ++index) {
    if (web_state_wrappers_[index]->web_state() == web_state)
      return index;
  }
  return kInvalidIndex;
}

int WebStateList::GetIndexOfWebStateWithURL(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (int index = 0; index < count(); ++index) {
    if (web_state_wrappers_[index]->web_state()->GetVisibleURL() == url)
      return index;
  }
  return kInvalidIndex;
}

int WebStateList::GetIndexOfInactiveWebStateWithURL(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (int index = 0; index < count(); ++index) {
    if (index == active_index_)
      continue;
    if (web_state_wrappers_[index]->web_state()->GetVisibleURL() == url)
      return index;
  }
  return kInvalidIndex;
}

WebStateOpener WebStateList::GetOpenerOfWebStateAt(int index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ContainsIndex(index));
  return web_state_wrappers_[index]->opener();
}

void WebStateList::SetOpenerOfWebStateAt(int index, WebStateOpener opener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ContainsIndex(index));
  DCHECK(ContainsIndex(GetIndexOfWebState(opener.opener)));
  web_state_wrappers_[index]->SetOpener(opener);
}

int WebStateList::GetIndexOfNextWebStateOpenedBy(const web::WebState* opener,
                                                 int start_index,
                                                 bool use_group) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetIndexOfNthWebStateOpenedBy(opener, start_index, use_group, 1);
}

int WebStateList::GetIndexOfLastWebStateOpenedBy(const web::WebState* opener,
                                                 int start_index,
                                                 bool use_group) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetIndexOfNthWebStateOpenedBy(opener, start_index, use_group, INT_MAX);
}

int WebStateList::InsertWebState(int index,
                                 std::unique_ptr<web::WebState> web_state,
                                 int insertion_flags,
                                 WebStateOpener opener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lock = LockForMutation();
  return InsertWebStateImpl(index, std::move(web_state), insertion_flags,
                            opener);
}

void WebStateList::MoveWebStateAt(int from_index, int to_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lock = LockForMutation();
  return MoveWebStateAtImpl(from_index, to_index);
}

std::unique_ptr<web::WebState> WebStateList::ReplaceWebStateAt(
    int index,
    std::unique_ptr<web::WebState> web_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(web_state.get(), nullptr);
  auto lock = LockForMutation();
  return ReplaceWebStateAtImpl(index, std::move(web_state));
}

std::unique_ptr<web::WebState> WebStateList::DetachWebStateAt(int index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lock = LockForMutation();
  return DetachWebStateAtImpl(index);
}

void WebStateList::CloseWebStateAt(int index, int close_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lock = LockForMutation();
  return CloseWebStateAtImpl(index, close_flags);
}

void WebStateList::CloseAllWebStates(int close_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lock = LockForMutation();
  return CloseAllWebStatesImpl(close_flags);
}

void WebStateList::ActivateWebStateAt(int index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ContainsIndex(index));
  auto lock = LockForMutation();
  return ActivateWebStateAtImpl(index, ActiveWebStateChangeReason::Activated);
}

base::AutoReset<bool> WebStateList::LockForMutation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!locked_) << "WebStateList is not re-entrant; it is an error to try to "
                  << "mutate it from one of the observers (even indirectly).";

  return base::AutoReset<bool>(&locked_, /*locked=*/true);
}

int WebStateList::InsertWebStateImpl(int index,
                                     std::unique_ptr<web::WebState> web_state,
                                     int insertion_flags,
                                     WebStateOpener opener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  DCHECK(web_state);
  const bool activating = IsInsertionFlagSet(insertion_flags, INSERT_ACTIVATE);

  if (IsInsertionFlagSet(insertion_flags, INSERT_INHERIT_OPENER))
    opener = WebStateOpener(GetActiveWebState());

  if (!IsInsertionFlagSet(insertion_flags, INSERT_FORCE_INDEX)) {
    index = order_controller_->DetermineInsertionIndex(opener.opener);
    if (index < 0 || count() < index)
      index = count();
  }

  DCHECK(ContainsIndex(index) || index == count());
  delegate_->WillAddWebState(web_state.get());

  web::WebState* web_state_ptr = web_state.get();
  web_state_wrappers_.insert(
      web_state_wrappers_.begin() + index,
      std::make_unique<WebStateWrapper>(std::move(web_state)));

  if (active_index_ >= index)
    ++active_index_;

  for (auto& observer : observers_)
    observer.WebStateInsertedAt(this, web_state_ptr, index, activating);

  if (opener.opener)
    SetOpenerOfWebStateAt(index, opener);

  if (activating)
    ActivateWebStateAtImpl(index, ActiveWebStateChangeReason::Inserted);

  return index;
}

void WebStateList::MoveWebStateAtImpl(int from_index, int to_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  DCHECK(ContainsIndex(from_index));
  DCHECK(ContainsIndex(to_index));
  if (from_index == to_index)
    return;

  std::unique_ptr<WebStateWrapper> web_state_wrapper =
      std::move(web_state_wrappers_[from_index]);
  web::WebState* web_state = web_state_wrapper->web_state();
  web_state_wrappers_.erase(web_state_wrappers_.begin() + from_index);
  web_state_wrappers_.insert(web_state_wrappers_.begin() + to_index,
                             std::move(web_state_wrapper));

  if (active_index_ == from_index) {
    active_index_ = to_index;
  } else {
    int min = std::min(from_index, to_index);
    int max = std::max(from_index, to_index);
    int delta = from_index < to_index ? -1 : +1;
    if (min <= active_index_ && active_index_ <= max)
      active_index_ += delta;
  }

  for (auto& observer : observers_)
    observer.WebStateMoved(this, web_state, from_index, to_index);
}

std::unique_ptr<web::WebState> WebStateList::ReplaceWebStateAtImpl(
    int index,
    std::unique_ptr<web::WebState> web_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  DCHECK(web_state);
  DCHECK(ContainsIndex(index));
  delegate_->WillAddWebState(web_state.get());

  ClearOpenersReferencing(index);

  web::WebState* web_state_ptr = web_state.get();
  std::unique_ptr<web::WebState> old_web_state =
      web_state_wrappers_[index]->ReplaceWebState(std::move(web_state));

  for (auto& observer : observers_) {
    observer.WebStateReplacedAt(this, old_web_state.get(), web_state_ptr,
                                index);
  }

  // When the active WebState is replaced, notify the observers as nearly
  // all of them needs to treat a replacement as the selection changed.
  NotifyIfActiveWebStateChanged(old_web_state.get(),
                                ActiveWebStateChangeReason::Replaced);

  delegate_->WebStateDetached(old_web_state.get());
  return old_web_state;
}

std::unique_ptr<web::WebState> WebStateList::DetachWebStateAtImpl(int index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  DCHECK(ContainsIndex(index));
  web::WebState* web_state = web_state_wrappers_[index]->web_state();
  for (auto& observer : observers_)
    observer.WillDetachWebStateAt(this, web_state, index);

  // Update the active index to prevent observer from seeing an invalid WebState
  // as the active one but only send the WebStateActivatedAt notification after
  // the WebStateDetachedAt one.
  const bool active_web_state_was_closed = (index == active_index_);
  active_index_ =
      order_controller_->DetermineNewActiveIndex(active_index_, index);

  ClearOpenersReferencing(index);
  std::unique_ptr<web::WebState> detached_web_state =
      web_state_wrappers_[index]->ReleaseWebState();
  web_state_wrappers_.erase(web_state_wrappers_.begin() + index);

  // Check that the active element (if there is one) is valid.
  DCHECK(active_index_ == kInvalidIndex || ContainsIndex(active_index_));

  for (auto& observer : observers_)
    observer.WebStateDetachedAt(this, web_state, index);

  if (active_web_state_was_closed) {
    NotifyIfActiveWebStateChanged(web_state,
                                  ActiveWebStateChangeReason::Closed);
  }

  delegate_->WebStateDetached(web_state);
  return detached_web_state;
}

void WebStateList::CloseWebStateAtImpl(int index, int close_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  std::unique_ptr<web::WebState> detached_web_state =
      DetachWebStateAtImpl(index);
  const bool user_action = IsClosingFlagSet(close_flags, CLOSE_USER_ACTION);
  for (auto& observer : observers_) {
    observer.WillCloseWebStateAt(this, detached_web_state.get(), index,
                                 user_action);
  }

  // Dropping detached_web_state will destroy it.
}

void WebStateList::CloseAllWebStatesImpl(int close_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);

  PerformBatchOperation(base::BindOnce(
      [](int close_flags, WebStateList* web_state_list) {
        // Since all the WebStates will be closed, notify that the active
        // WebState is de-activated before closing them. This avoid sending
        // one notification per WebState in the worst case (when the active
        // WebState is the last one and no opener is set to any WebState).
        web_state_list->ActivateWebStateAtImpl(
            kInvalidIndex, ActiveWebStateChangeReason::Closed);

        // Close the WebStates from last to first.
        while (!web_state_list->empty())
          web_state_list->CloseWebStateAtImpl(web_state_list->count() - 1,
                                              close_flags);
      },
      close_flags));
}

void WebStateList::ActivateWebStateAtImpl(int index,
                                          ActiveWebStateChangeReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  DCHECK(ContainsIndex(index) || index == kInvalidIndex);
  web::WebState* old_web_state = GetActiveWebState();
  active_index_ = index;
  NotifyIfActiveWebStateChanged(old_web_state, reason);
}

void WebStateList::AddObserver(WebStateListObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void WebStateList::RemoveObserver(WebStateListObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void WebStateList::PerformBatchOperation(
    base::OnceCallback<void(WebStateList*)> operation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!batch_operation_in_progress_);
  base::AutoReset<bool> lock(&batch_operation_in_progress_, /*locked=*/true);

  for (auto& observer : observers_)
    observer.WillBeginBatchOperation(this);
  if (!operation.is_null())
    std::move(operation).Run(this);
  for (auto& observer : observers_)
    observer.BatchOperationEnded(this);
}

void WebStateList::ClearOpenersReferencing(int index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  web::WebState* old_web_state = web_state_wrappers_[index]->web_state();
  for (auto& web_state_wrapper : web_state_wrappers_) {
    if (web_state_wrapper->opener().opener == old_web_state)
      web_state_wrapper->SetOpener(WebStateOpener());
  }
}

void WebStateList::NotifyIfActiveWebStateChanged(
    web::WebState* old_web_state,
    ActiveWebStateChangeReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  web::WebState* new_web_state = GetActiveWebState();
  if (old_web_state == new_web_state)
    return;

  for (auto& observer : observers_) {
    observer.WebStateActivatedAt(this, old_web_state, new_web_state,
                                 active_index_, reason);
  }
}

int WebStateList::GetIndexOfNthWebStateOpenedBy(const web::WebState* opener,
                                                int start_index,
                                                bool use_group,
                                                int n) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(n, 0);
  if (!opener || !ContainsIndex(start_index))
    return kInvalidIndex;

  const int opener_navigation_index =
      use_group ? opener->GetNavigationManager()->GetLastCommittedItemIndex()
                : -1;

  int found_index = kInvalidIndex;
  const int list_length = count();
  for (int i = 1; i < list_length; ++i) {
    const int index = (start_index + i) % list_length;
    DCHECK_NE(index, start_index);

    const auto& wrapper = web_state_wrappers_[index];
    if (!wrapper->WasOpenedBy(opener, opener_navigation_index, use_group))
      continue;

    found_index = index;
    if (--n == 0)
      break;
  }

  return found_index;
}

// static
const int WebStateList::kInvalidIndex;
