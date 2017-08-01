// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlsWindowEventListener_h
#define MediaControlsWindowEventListener_h

#include "core/events/EventListener.h"
#include "platform/wtf/Functional.h"

namespace blink {

class MediaControlsImpl;

class MediaControlsWindowEventListener final : public EventListener {
 public:
  using Callback = Function<void(), WTF::kSameThreadAffinity>;

  static MediaControlsWindowEventListener* Create(MediaControlsImpl*, Callback);

  bool operator==(const EventListener&) const override;

  void Start();
  void Stop();

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit MediaControlsWindowEventListener(MediaControlsImpl*, Callback);

  void handleEvent(ExecutionContext*, Event*) override;

  Member<MediaControlsImpl> media_controls_;
  Callback callback_;
  bool is_active_;
};

}  // namespace blink

#endif  // MediaControlsWindowEventListener_h
