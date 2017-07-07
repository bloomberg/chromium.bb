// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenWakeLock_h
#define ScreenWakeLock_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/frame/LocalFrame.h"
#include "core/page/PageVisibilityObserver.h"
#include "modules/ModulesExport.h"
#include "platform/wtf/Noncopyable.h"
#include "services/device/public/interfaces/wake_lock.mojom-blink.h"

namespace blink {

class LocalFrame;
class Screen;

class MODULES_EXPORT ScreenWakeLock final
    : public GarbageCollectedFinalized<ScreenWakeLock>,
      public Supplement<LocalFrame>,
      public ContextLifecycleObserver,
      public PageVisibilityObserver {
  USING_GARBAGE_COLLECTED_MIXIN(ScreenWakeLock);
  WTF_MAKE_NONCOPYABLE(ScreenWakeLock);

 public:
  static bool keepAwake(Screen&);
  static void setKeepAwake(Screen&, bool);

  static const char* SupplementName();
  static ScreenWakeLock* From(LocalFrame*);

  ~ScreenWakeLock() = default;

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit ScreenWakeLock(LocalFrame&);

  // Inherited from PageVisibilityObserver.
  void PageVisibilityChanged() override;

  // Inherited from ContextLifecycleObserver.
  void ContextDestroyed(ExecutionContext*) override;

  bool keepAwake() const;
  void setKeepAwake(bool);

  static ScreenWakeLock* FromScreen(Screen&);
  void NotifyService();

  device::mojom::blink::WakeLockPtr service_;
  bool keep_awake_;
};

}  // namespace blink

#endif  // ScreenWakeLock_h
