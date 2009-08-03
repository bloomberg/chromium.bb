/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file implements unit tests for class EventManager.

#include "tests/common/win/testing_common.h"
#include "core/cross/event_manager.h"

namespace o3d {

class EventManagerTest : public testing::Test {
 protected:
  EventManagerTest() {
  }

  virtual void SetUp();
  virtual void TearDown();

  EventManager event_manager_;
};

void EventManagerTest::SetUp() {
}

void EventManagerTest::TearDown() {
  event_manager_.ClearAll();
}

TEST_F(EventManagerTest, CanClearAllFromEventCallback) {
  class ClearAllEventCallback : public EventCallback {
   public:
    explicit ClearAllEventCallback(EventManager* event_manager)
        : event_manager_(event_manager) {
    }
    virtual void Run(const Event& event) {
      event_manager_->ClearAll();
    }
   private:
    EventManager* event_manager_;
  };

  event_manager_.SetEventCallback(
      Event::TYPE_CLICK,
      new ClearAllEventCallback(&event_manager_));
  Event event(Event::TYPE_CLICK);
  event_manager_.AddEventToQueue(event);
  event_manager_.ProcessQueue();
}
}  // namespace o3d
