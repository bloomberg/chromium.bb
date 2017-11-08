// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlatformEventController_h
#define PlatformEventController_h

#include "core/CoreExport.h"
#include "core/page/PageVisibilityObserver.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "public/platform/TaskType.h"

namespace blink {

// Base controller class for registering controllers with a dispatcher.
// It watches page visibility and calls stopUpdating when page is not visible.
// It provides a didUpdateData() callback method which is called when new data
// it available.
class CORE_EXPORT PlatformEventController : public PageVisibilityObserver {
 public:
  void StartUpdating();
  void StopUpdating();

  // This is called when new data becomes available.
  virtual void DidUpdateData() = 0;

  virtual void Trace(blink::Visitor*);

 protected:
  explicit PlatformEventController(Document*);
  virtual ~PlatformEventController();

  virtual void RegisterWithDispatcher() = 0;
  virtual void UnregisterWithDispatcher() = 0;

  // When true initiates a one-shot didUpdateData() when startUpdating() is
  // called.
  virtual bool HasLastData() = 0;

  bool has_event_listener_;

 private:
  // Inherited from PageVisibilityObserver.
  void PageVisibilityChanged() override;

  void UpdateCallback();

  bool is_active_;
  Member<Document> document_;
  TaskHandle update_callback_handle_;
};

}  // namespace blink

#endif  // PlatformEventController_h
