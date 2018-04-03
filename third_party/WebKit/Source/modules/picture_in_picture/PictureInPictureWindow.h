// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PictureInPictureWindow_h
#define PictureInPictureWindow_h

#include "core/execution_context/ExecutionContext.h"
#include "modules/EventTargetModules.h"
#include "platform/heap/Handle.h"

namespace blink {

// The PictureInPictureWindow is meant to be used only by
// PictureInPictureController and is fundamentally just a simple proxy to get
// information such as dimensions about the current Picture-in-Picture window.
class PictureInPictureWindow : public EventTargetWithInlineData,
                               public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(PictureInPictureWindow);
  DEFINE_WRAPPERTYPEINFO();

 public:
  PictureInPictureWindow(ExecutionContext*, int width, int height);

  int width() const { return width_; }
  int height() const { return height_; }

  // Called when Picture-in-Picture window state is closed.
  void OnClose();

  DEFINE_ATTRIBUTE_EVENT_LISTENER(resize);

  // EventTarget overrides.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override {
    return ContextClient::GetExecutionContext();
  }

  void Trace(blink::Visitor*) override;

 private:
  // The Picture-in-Picture window width in pixels.
  int width_;

  // The Picture-in-Picture window height in pixels.
  int height_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PictureInPictureWindow);
};

}  // namespace blink

#endif  // PictureInPictureWindow_h
