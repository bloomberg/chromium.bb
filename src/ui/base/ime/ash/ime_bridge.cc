// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/ime_bridge.h"

namespace ui {

static IMEBridge* g_ime_bridge = nullptr;

IMEBridge::IMEBridge()
    : current_input_context_(ui::TEXT_INPUT_TYPE_NONE,
                             ui::TEXT_INPUT_MODE_DEFAULT,
                             0,
                             ui::TextInputClient::FOCUS_REASON_NONE,
                             false /* should_do_learning */) {}

IMEBridge::~IMEBridge() = default;

IMEInputContextHandlerInterface* IMEBridge::GetInputContextHandler() const {
  return input_context_handler_;
}

void IMEBridge::SetInputContextHandler(
    IMEInputContextHandlerInterface* handler) {
  input_context_handler_ = handler;
  for (auto& observer : observers_)
    observer.OnInputContextHandlerChanged();
}

void IMEBridge::SetCurrentEngineHandler(IMEEngineHandlerInterface* handler) {
  engine_handler_ = handler;
}

IMEEngineHandlerInterface* IMEBridge::GetCurrentEngineHandler() const {
  return engine_handler_;
}

void IMEBridge::SetCurrentInputContext(
    const IMEEngineHandlerInterface::InputContext& input_context) {
  current_input_context_ = input_context;
}

const IMEEngineHandlerInterface::InputContext&
IMEBridge::GetCurrentInputContext() const {
  return current_input_context_;
}

void IMEBridge::AddObserver(ui::IMEBridgeObserver* observer) {
  observers_.AddObserver(observer);
}

void IMEBridge::RemoveObserver(ui::IMEBridgeObserver* observer) {
  observers_.RemoveObserver(observer);
}

void IMEBridge::SetCandidateWindowHandler(
    ash::IMECandidateWindowHandlerInterface* handler) {
  candidate_window_handler_ = handler;
}

ash::IMECandidateWindowHandlerInterface* IMEBridge::GetCandidateWindowHandler()
    const {
  return candidate_window_handler_;
}

void IMEBridge::SetAssistiveWindowHandler(
    ash::IMEAssistiveWindowHandlerInterface* handler) {
  assistive_window_handler_ = handler;
}

ash::IMEAssistiveWindowHandlerInterface* IMEBridge::GetAssistiveWindowHandler()
    const {
  return assistive_window_handler_;
}

// static.
IMEBridge* IMEBridge::Get() {
  if (!g_ime_bridge) {
    g_ime_bridge = new IMEBridge();
  }
  return g_ime_bridge;
}

}  // namespace ui
