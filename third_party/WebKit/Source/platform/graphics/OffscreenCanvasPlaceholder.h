// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvasPlaceholder_h
#define OffscreenCanvasPlaceholder_h

#include <memory>
#include "platform/PlatformExport.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/WeakPtr.h"

namespace blink {

class OffscreenCanvasFrameDispatcher;
class StaticBitmapImage;
class WebTaskRunner;

class PLATFORM_EXPORT OffscreenCanvasPlaceholder {
 public:
  ~OffscreenCanvasPlaceholder();

  virtual void SetPlaceholderFrame(RefPtr<StaticBitmapImage>,
                                   WeakPtr<OffscreenCanvasFrameDispatcher>,
                                   RefPtr<WebTaskRunner>,
                                   unsigned resource_id);
  void ReleasePlaceholderFrame();

  void SetSuspendOffscreenCanvasAnimation(bool);

  static OffscreenCanvasPlaceholder* GetPlaceholderById(
      unsigned placeholder_id);

  void RegisterPlaceholder(unsigned placeholder_id);
  void UnregisterPlaceholder();
  const RefPtr<StaticBitmapImage>& PlaceholderFrame() const {
    return placeholder_frame_;
  }

  bool IsPlaceholderRegistered() const {
    return placeholder_id_ != kNoPlaceholderId;
  }

 private:
  bool PostSetSuspendAnimationToOffscreenCanvasThread(bool suspend);

  RefPtr<StaticBitmapImage> placeholder_frame_;
  WeakPtr<OffscreenCanvasFrameDispatcher> frame_dispatcher_;
  RefPtr<WebTaskRunner> frame_dispatcher_task_runner_;
  unsigned placeholder_frame_resource_id_ = 0;

  enum {
    kNoPlaceholderId = -1,
  };
  int placeholder_id_ = kNoPlaceholderId;

  enum AnimationState {
    kActiveAnimation,
    kSuspendedAnimation,
    kShouldSuspendAnimation,
    kShouldActivateAnimation,
  };
  AnimationState animation_state_ = kActiveAnimation;
};

}  // blink

#endif
