// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_cursor_client.h"

#include "ui/aura/client/cursor_client_observer.h"

namespace aura {
namespace test {

TestCursorClient::TestCursorClient(aura::Window* root_window)
    : visible_(true),
      mouse_events_enabled_(true),
      root_window_(root_window) {
  client::SetCursorClient(root_window, this);
}

TestCursorClient::~TestCursorClient() {
  client::SetCursorClient(root_window_, NULL);
}

void TestCursorClient::SetCursor(gfx::NativeCursor cursor) {
}

void TestCursorClient::ShowCursor() {
  visible_ = true;
  FOR_EACH_OBSERVER(aura::client::CursorClientObserver, observers_,
                    OnCursorVisibilityChanged(true));
}

void TestCursorClient::HideCursor() {
  visible_ = false;
  FOR_EACH_OBSERVER(aura::client::CursorClientObserver, observers_,
                    OnCursorVisibilityChanged(false));
}

void TestCursorClient::SetScale(float scale) {
}

bool TestCursorClient::IsCursorVisible() const {
  return visible_;
}

void TestCursorClient::EnableMouseEvents() {
  mouse_events_enabled_ = true;
}

void TestCursorClient::DisableMouseEvents() {
  mouse_events_enabled_ = false;
}

bool TestCursorClient::IsMouseEventsEnabled() const {
  return mouse_events_enabled_;
}

void TestCursorClient::SetDisplay(const gfx::Display& display) {
}

void TestCursorClient::LockCursor() {
}

void TestCursorClient::UnlockCursor() {
}

void TestCursorClient::AddObserver(
    aura::client::CursorClientObserver* observer) {
  observers_.AddObserver(observer);
}

void TestCursorClient::RemoveObserver(
    aura::client::CursorClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace test
}  // namespace aura
