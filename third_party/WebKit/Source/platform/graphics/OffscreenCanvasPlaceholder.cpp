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
    RefPtr<blink::Image> oldImage,
    unsigned resourceId) {
  oldImage = nullptr;  // Needed to unref'ed on the right thread
  if (dispatcher) {
    dispatcher->ReclaimResource(resourceId);
  }
}

}  // unnamed namespace

namespace blink {

OffscreenCanvasPlaceholder::~OffscreenCanvasPlaceholder() {
  UnregisterPlaceholder();
}

OffscreenCanvasPlaceholder* OffscreenCanvasPlaceholder::GetPlaceholderById(
    unsigned placeholder_id) {
  PlaceholderIdMap::iterator it = placeholderRegistry().Find(placeholder_id);
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
  DCHECK(placeholderRegistry().Find(placeholder_id_)->value == this);
  placeholderRegistry().erase(placeholder_id_);
  placeholder_id_ = kNoPlaceholderId;
}

void OffscreenCanvasPlaceholder::SetPlaceholderFrame(
    RefPtr<StaticBitmapImage> new_frame,
    WeakPtr<OffscreenCanvasFrameDispatcher> dispatcher,
    RefPtr<WebTaskRunner> task_runner,
    unsigned resource_id) {
  DCHECK(IsPlaceholderRegistered());
  DCHECK(new_frame);
  ReleasePlaceholderFrame();
  placeholder_frame_ = std::move(new_frame);
  frame_dispatcher_ = std::move(dispatcher);
  frame_dispatcher_task_runner_ = std::move(task_runner);
  placeholder_frame_resource_id_ = resource_id;
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

}  // blink
