// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_IME_BRIDGE_H_
#define UI_BASE_IME_ASH_IME_BRIDGE_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "ui/base/ime/ash/ime_assistive_window_handler_interface.h"
#include "ui/base/ime/ash/ime_bridge_observer.h"
#include "ui/base/ime/ash/ime_candidate_window_handler_interface.h"
#include "ui/base/ime/ash/ime_engine_handler_interface.h"
#include "ui/base/ime/ash/ime_input_context_handler_interface.h"

class IMECandidateWindowHandlerInterface;
class IMEAssistiveWindowHandlerInterface;

namespace ui {

// IMEBridge provides access of each IME related handler. This class
// is used for IME implementation.
class COMPONENT_EXPORT(UI_BASE_IME_ASH) IMEBridge {
 public:
  IMEBridge(const IMEBridge&) = delete;
  IMEBridge& operator=(const IMEBridge&) = delete;
  ~IMEBridge();

  // Constructs the global singleton (if not available yet) then returns it.
  // TODO(crbug/1279743): Use dependency injection instead of global singleton.
  static IMEBridge* Get();

  // Returns current InputContextHandler. This function returns nullptr if input
  // context is not ready to use.
  IMEInputContextHandlerInterface* GetInputContextHandler() const;

  // Updates current InputContextHandler. If there is no active input context,
  // pass nullptr for |handler|. Caller must release |handler|.
  void SetInputContextHandler(IMEInputContextHandlerInterface* handler);

  // Updates current EngineHandler. If there is no active engine service, pass
  // nullptr for |handler|. Caller must release |handler|.
  void SetCurrentEngineHandler(IMEEngineHandlerInterface* handler);

  // Returns current EngineHandler. This function returns nullptr if current
  // engine is not ready to use.
  IMEEngineHandlerInterface* GetCurrentEngineHandler() const;

  // Updates the current input context.
  // This is called from `InputMethodAsh`.
  void SetCurrentInputContext(
      const IMEEngineHandlerInterface::InputContext& input_context);

  // Returns the current input context.
  // This is called from InputMethodEngine.
  const IMEEngineHandlerInterface::InputContext& GetCurrentInputContext() const;

  // Add or remove observers of events such as switching engines, etc.
  void AddObserver(ui::IMEBridgeObserver* observer);
  void RemoveObserver(ui::IMEBridgeObserver* observer);

  // Returns current CandidateWindowHandler. This function returns nullptr if
  // current candidate window is not ready to use.
  ash::IMECandidateWindowHandlerInterface* GetCandidateWindowHandler() const;

  // Updates current CandidatWindowHandler. If there is no active candidate
  // window service, pass nullptr for |handler|. Caller must release |handler|.
  void SetCandidateWindowHandler(
      ash::IMECandidateWindowHandlerInterface* handler);

  ash::IMEAssistiveWindowHandlerInterface* GetAssistiveWindowHandler() const;
  void SetAssistiveWindowHandler(
      ash::IMEAssistiveWindowHandlerInterface* handler);

 private:
  IMEBridge();

  IMEInputContextHandlerInterface* input_context_handler_ = nullptr;
  IMEEngineHandlerInterface* engine_handler_ = nullptr;
  base::ObserverList<IMEBridgeObserver> observers_;
  IMEEngineHandlerInterface::InputContext current_input_context_;

  ash::IMECandidateWindowHandlerInterface* candidate_window_handler_ = nullptr;
  ash::IMEAssistiveWindowHandlerInterface* assistive_window_handler_ = nullptr;
};

}  // namespace ui

#endif  // UI_BASE_IME_ASH_IME_BRIDGE_H_
