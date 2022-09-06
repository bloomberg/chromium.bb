// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_list_order_controller.h"

#include <cstdint>
#include <set>

#include "base/check_op.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_removing_indexes.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Find the index of next non-removed WebState opened by |web_state|. It
// may return WebStateList::kInvalidIndex if there is no such indexes.
int FindIndexOfNextNonRemovedWebStateOpenedBy(
    const WebStateListRemovingIndexes& removing_indexes,
    const WebStateList& web_state_list,
    const web::WebState* web_state,
    int starting_index) {
  std::set<int> children;
  for (;;) {
    const int child_index = web_state_list.GetIndexOfNextWebStateOpenedBy(
        web_state, starting_index, false);

    // The active WebState has no child, fall back to the next heuristic.
    if (child_index == WebStateList::kInvalidIndex)
      break;

    // All children are going to be removed, fallback to the next heuristic.
    if (children.find(child_index) != children.end())
      break;

    // Found a child that is not removed, select it as the next active
    // WebState.
    const int child_index_after_removal =
        removing_indexes.IndexAfterRemoval(child_index);

    if (child_index_after_removal != WebStateList::kInvalidIndex)
      return child_index_after_removal;

    children.insert(child_index);
    starting_index = child_index;
  }

  return WebStateList::kInvalidIndex;
}

}  // anonymous namespace

WebStateListOrderController::WebStateListOrderController(
    const WebStateList& web_state_list)
    : web_state_list_(web_state_list) {}

WebStateListOrderController::~WebStateListOrderController() = default;

int WebStateListOrderController::DetermineInsertionIndex(
    const web::WebState* opener) const {
  if (!opener)
    return web_state_list_.count();

  int opener_index = web_state_list_.GetIndexOfWebState(opener);
  DCHECK_NE(WebStateList::kInvalidIndex, opener_index);

  const int list_child_index = web_state_list_.GetIndexOfLastWebStateOpenedBy(
      opener, opener_index, true);

  const int reference_index = list_child_index != WebStateList::kInvalidIndex
                                  ? list_child_index
                                  : opener_index;

  // Check for overflows (just a DCHECK as INT_MAX open WebState is unlikely).
  DCHECK_LT(reference_index, INT_MAX);
  return reference_index + 1;
}

int WebStateListOrderController::DetermineNewActiveIndex(
    int active_index,
    WebStateListRemovingIndexes removing_indexes) const {
  // If there is no active element, then there will be no new active element
  // after closing the element.
  if (active_index == WebStateList::kInvalidIndex)
    return WebStateList::kInvalidIndex;

  // Otherwise the index needs to be valid.
  DCHECK(web_state_list_.ContainsIndex(active_index));

  // If the elements removed are the the sole remaining elements in the
  // WebStateList, clear the selection (as the list will be empty).
  const int count = web_state_list_.count();
  if (count == removing_indexes.count())
    return WebStateList::kInvalidIndex;

  // If the active element is not removed, then the active element won't change
  // but its index may need to be tweaked if after some of the removed elements.
  const int active_index_after_removal =
      removing_indexes.IndexAfterRemoval(active_index);
  if (active_index_after_removal != WebStateList::kInvalidIndex)
    return active_index_after_removal;

  // Check if any of the "child" of the active WebState can be selected to be
  // the new active element. Prefer childs located after the active element,
  // but this may end up selecting an element before it.
  const int child_index_after_removal =
      FindIndexOfNextNonRemovedWebStateOpenedBy(
          removing_indexes, web_state_list_,
          web_state_list_.GetWebStateAt(active_index), active_index);
  if (child_index_after_removal != WebStateList::kInvalidIndex)
    return child_index_after_removal;

  const WebStateOpener opener =
      web_state_list_.GetOpenerOfWebStateAt(active_index);
  if (opener.opener) {
    // Check if any of the "sibling" of the active WebState can be selected
    // to be the new active element. Prefer siblings located after the active
    // element, but this may end up selecting an element before it.
    const int sibling_index_after_removal =
        FindIndexOfNextNonRemovedWebStateOpenedBy(
            removing_indexes, web_state_list_, opener.opener, active_index);
    if (sibling_index_after_removal != WebStateList::kInvalidIndex)
      return sibling_index_after_removal;

    // If the opener is not removed, select it as the next WebState.
    const int opener_index_after_removal = removing_indexes.IndexAfterRemoval(
        web_state_list_.GetIndexOfWebState(opener.opener));
    if (opener_index_after_removal != WebStateList::kInvalidIndex)
      return opener_index_after_removal;
  }

  // Look for the closest non-removed WebState after the active WebState, or
  // if none, use the closest non-removed WebState before the active WebState.
  for (int index = active_index + 1; index < count; ++index) {
    const int index_after_removal = removing_indexes.IndexAfterRemoval(index);
    if (index_after_removal != WebStateList::kInvalidIndex)
      return index_after_removal;
  }

  for (int index = active_index - 1; index >= 0; --index) {
    const int index_after_removal = removing_indexes.IndexAfterRemoval(index);
    if (index_after_removal != WebStateList::kInvalidIndex)
      return index_after_removal;
  }

  NOTREACHED() << "No active WebState selected by WebStateList not empty";
  return WebStateList::kInvalidIndex;
}
