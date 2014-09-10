// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_TEST_EVENT_PROCESSOR_H_
#define UI_EVENTS_TEST_TEST_EVENT_PROCESSOR_H_

#include "base/memory/scoped_ptr.h"
#include "ui/events/event_processor.h"

namespace ui {
namespace test {

class TestEventProcessor : public EventProcessor {
 public:
  TestEventProcessor();
  virtual ~TestEventProcessor();

  int num_times_processing_finished() const {
    return num_times_processing_finished_;
  }

  void SetRoot(scoped_ptr<EventTarget> root);
  void ResetCounts();

  // EventProcessor:
  virtual bool CanDispatchToTarget(EventTarget* target) OVERRIDE;
  virtual EventTarget* GetRootTarget() OVERRIDE;
  virtual EventDispatchDetails OnEventFromSource(Event* event) OVERRIDE;
  virtual void OnEventProcessingFinished(Event* event) OVERRIDE;

 private:
  scoped_ptr<EventTarget> root_;

  // Counts the number of times OnEventProcessingFinished() has been called.
  int num_times_processing_finished_;

  DISALLOW_COPY_AND_ASSIGN(TestEventProcessor);
};

}  // namespace test
}  // namespace ui

#endif  // UI_EVENTS_TEST_TEST_EVENT_PROCESSOR_H_
