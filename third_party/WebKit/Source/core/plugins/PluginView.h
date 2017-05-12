/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2014 Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef PluginView_h
#define PluginView_h

#include "core/CoreExport.h"
#include "core/frame/FrameOrPlugin.h"
#include "platform/geometry/IntRect.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebFocusType.h"
#include "v8/include/v8.h"

namespace blink {

class Event;
class FrameView;
class ResourceResponse;
class WebLayer;

// TODO(joelhockey): Remove this class.
// The only implementation of this class is web/WebPluginContainerImpl.
// It can be used directly.
class CORE_EXPORT PluginView : public FrameOrPlugin {
 public:
  virtual ~PluginView() {}

  bool IsPluginView() const override { return true; }

  virtual void SetParent(FrameView*) = 0;
  virtual FrameView* Parent() const = 0;
  virtual void SetParentVisible(bool) = 0;
  virtual void SetFocused(bool, WebFocusType) = 0;
  virtual void FrameRectsChanged() = 0;
  virtual void GeometryMayHaveChanged() = 0;
  virtual void HandleEvent(Event*) = 0;
  virtual void EventListenersRemoved() = 0;
  virtual bool IsPluginContainer() const { return false; }
  virtual bool IsErrorplaceholder() { return false; }

  virtual WebLayer* PlatformLayer() const { return 0; }
  virtual v8::Local<v8::Object> ScriptableObject(v8::Isolate*) {
    return v8::Local<v8::Object>();
  }
  virtual bool WantsWheelEvents() { return false; }
  virtual bool SupportsKeyboardFocus() const { return false; }
  virtual bool SupportsInputMethod() const { return false; }
  virtual bool CanProcessDrag() const { return false; }

  virtual void DidReceiveResponse(const ResourceResponse&) {}
  virtual void DidReceiveData(const char*, int) {}

  virtual void UpdateAllLifecyclePhases() {}
  virtual void InvalidatePaint() {}
};

DEFINE_TYPE_CASTS(PluginView,
                  FrameOrPlugin,
                  frame_or_plugin,
                  frame_or_plugin->IsPluginView(),
                  frame_or_plugin.IsPluginView());

}  // namespace blink

#endif  // PluginView_h
