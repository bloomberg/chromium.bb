// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlsWindowEventListener_h
#define MediaControlsWindowEventListener_h

#include "base/callback.h"
#include "core/dom/events/EventListener.h"

namespace blink {

class MediaControlsImpl;

class MediaControlsWindowEventListener final : public EventListener {
 public:
  using Callback = base::RepeatingCallback<void()>;

  static MediaControlsWindowEventListener* Create(MediaControlsImpl*, Callback);

  bool operator==(const EventListener&) const override;

  void Start();
  void Stop();

  virtual void Trace(blink::Visitor*);

 private:
  explicit MediaControlsWindowEventListener(MediaControlsImpl*, Callback);

  void handleEvent(ExecutionContext*, Event*) override;

  Member<MediaControlsImpl> media_controls_;
  Callback callback_;
  bool is_active_;
};

}  // namespace blink

#endif  // MediaControlsWindowEventListener_h
