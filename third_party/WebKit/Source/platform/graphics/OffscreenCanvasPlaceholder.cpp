// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/OffscreenCanvasPlaceholder.h"

#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "platform/graphics/OffscreenCanvasFrameDispatcher.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/wtf/HashMap.h"

namespace {

typedef HashMap<int, blink::OffscreenCanvasPlaceholder*> PlaceholderIdMap;

PlaceholderIdMap& placeholderRegistry() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(PlaceholderIdMap, s_placeholderRegistry, ());
  return s_placeholderRegistry;
}

void releaseFrameToDispatcher(
    WeakPtr<blink::OffscreenCanvasFrameDispatcher> dispatcher,
    scoped_refptr<blink::Image> oldImage,
    unsigned resourceId) {
  oldImage = nullptr;  // Needed to unref'ed on the right thread
  if (dispatcher) {
    dispatcher->ReclaimResource(resourceId);
  }
}

void SetSuspendAnimation(
    WeakPtr<blink::OffscreenCanvasFrameDispatcher> dispatcher,
    bool suspend) {
  if (dispatcher) {
    dispatcher->SetSuspendAnimation(suspend);
  }
}

}  // unnamed namespace

namespace blink {

OffscreenCanvasPlaceholder::~OffscreenCanvasPlaceholder() {
  UnregisterPlaceholder();
}

OffscreenCanvasPlaceholder* OffscreenCanvasPlaceholder::GetPlaceholderById(
    unsigned placeholder_id) {
  PlaceholderIdMap::iterator it = placeholderRegistry().find(placeholder_id);
  if (it == placeholderRegistry().end())
    return nullptr;
  return it->value;
}

void OffscreenCanvasPlaceholder::RegisterPlaceholder(unsigned placeholder_id) {
  DCHECK(!placeholderRegistry().Contains(placeholder_id));
  DCHECK(!IsPlaceholderRegistered());
  placeholderRegistry().insert(placeholder_id, this);
  placeholder_id_ = placeholder_id;
}

void OffscreenCanvasPlaceholder::UnregisterPlaceholder() {
  if (!IsPlaceholderRegistered())
    return;
  DCHECK(placeholderRegistry().find(placeholder_id_)->value == this);
  placeholderRegistry().erase(placeholder_id_);
  placeholder_id_ = kNoPlaceholderId;
}

void OffscreenCanvasPlaceholder::SetPlaceholderFrame(
    scoped_refptr<StaticBitmapImage> new_frame,
    WeakPtr<OffscreenCanvasFrameDispatcher> dispatcher,
    scoped_refptr<WebTaskRunner> task_runner,
    unsigned resource_id) {
  DCHECK(IsPlaceholderRegistered());
  DCHECK(new_frame);
  ReleasePlaceholderFrame();
  placeholder_frame_ = std::move(new_frame);
  frame_dispatcher_ = std::move(dispatcher);
  frame_dispatcher_task_runner_ = std::move(task_runner);
  placeholder_frame_resource_id_ = resource_id;

  if (animation_state_ == kShouldSuspendAnimation) {
    bool success = PostSetSuspendAnimationToOffscreenCanvasThread(true);
    DCHECK(success);
    animation_state_ = kSuspendedAnimation;
  } else if (animation_state_ == kShouldActivateAnimation) {
    bool success = PostSetSuspendAnimationToOffscreenCanvasThread(false);
    DCHECK(success);
    animation_state_ = kActiveAnimation;
  }
}

void OffscreenCanvasPlaceholder::ReleasePlaceholderFrame() {
  DCHECK(IsPlaceholderRegistered());
  if (placeholder_frame_) {
    placeholder_frame_->Transfer();
    frame_dispatcher_task_runner_->PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(releaseFrameToDispatcher, std::move(frame_dispatcher_),
                        std::move(placeholder_frame_),
                        placeholder_frame_resource_id_));
  }
}

void OffscreenCanvasPlaceholder::SetSuspendOffscreenCanvasAnimation(
    bool suspend) {
  switch (animation_state_) {
    case kActiveAnimation:
      if (suspend) {
        if (PostSetSuspendAnimationToOffscreenCanvasThread(suspend)) {
          animation_state_ = kSuspendedAnimation;
        } else {
          animation_state_ = kShouldSuspendAnimation;
        }
      }
      break;
    case kSuspendedAnimation:
      if (!suspend) {
        if (PostSetSuspendAnimationToOffscreenCanvasThread(suspend)) {
          animation_state_ = kActiveAnimation;
        } else {
          animation_state_ = kShouldActivateAnimation;
        }
      }
      break;
    case kShouldSuspendAnimation:
      if (!suspend) {
        animation_state_ = kActiveAnimation;
      }
      break;
    case kShouldActivateAnimation:
      if (suspend) {
        animation_state_ = kSuspendedAnimation;
      }
      break;
    default:
      NOTREACHED();
  }
}

bool OffscreenCanvasPlaceholder::PostSetSuspendAnimationToOffscreenCanvasThread(
    bool suspend) {
  if (!frame_dispatcher_task_runner_)
    return false;
  frame_dispatcher_task_runner_->PostTask(
      BLINK_FROM_HERE,
      CrossThreadBind(SetSuspendAnimation, frame_dispatcher_, suspend));
  return true;
}

}  // blink
