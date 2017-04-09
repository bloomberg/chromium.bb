// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlsWindowEventListener_h
#define MediaControlsWindowEventListener_h

#include "core/CoreExport.h"
#include "core/events/EventListener.h"
#include "wtf/Functional.h"

namespace blink {

class MediaControls;

class CORE_EXPORT MediaControlsWindowEventListener final
    : public EventListener {
 public:
  using Callback = Function<void(), WTF::kSameThreadAffinity>;

  static MediaControlsWindowEventListener* Create(MediaControls*,
                                                  std::unique_ptr<Callback>);

  bool operator==(const EventListener&) const override;

  void Start();
  void Stop();

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit MediaControlsWindowEventListener(MediaControls*,
                                            std::unique_ptr<Callback>);

  void handleEvent(ExecutionContext*, Event*) override;

  Member<MediaControls> media_controls_;
  std::unique_ptr<Callback> callback_;
  bool is_active_;
};

}  // namespace blink

#endif  // MediaControlsWindowEventListener_h
