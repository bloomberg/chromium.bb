// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_RULE_BASED_ENGINE_H_
#define CHROMEOS_SERVICES_IME_RULE_BASED_ENGINE_H_

#include "chromeos/services/ime/input_engine.h"
#include "chromeos/services/ime/public/cpp/rulebased/engine.h"
#include "chromeos/services/ime/public/cpp/suggestions.h"
#include "chromeos/services/ime/public/mojom/input_method.mojom.h"
#include "chromeos/services/ime/public/mojom/input_method_host.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace ime {

// Handles rule-based input methods such as Arabic and Vietnamese.
// Rule-based input methods are based off deterministic rules and do not
// provide features such as suggestions.
class RuleBasedEngine : public InputEngine, public mojom::InputMethod {
 public:
  // Returns nullptr if |ime_spec| is not valid for this RuleBasedEngine.
  static std::unique_ptr<RuleBasedEngine> Create(
      const std::string& ime_spec,
      mojo::PendingReceiver<mojom::InputMethod> receiver,
      mojo::PendingRemote<mojom::InputMethodHost> host);

  RuleBasedEngine(const RuleBasedEngine& other) = delete;
  RuleBasedEngine& operator=(const RuleBasedEngine& other) = delete;
  ~RuleBasedEngine() override;

  // InputEngine:
  bool IsConnected() override;

  // mojom::InputMethod overrides:
  // Most of these methods are deliberately empty because rule-based input
  // methods do not need to listen to these events.
  void OnFocus(mojom::InputFieldInfoPtr input_field_info) override {}
  void OnBlur() override {}
  void OnSurroundingTextChanged(
      const std::string& text,
      uint32_t offset,
      mojom::SelectionRangePtr selection_range) override {}
  void OnCompositionCanceledBySystem() override;
  void ProcessKeyEvent(mojom::PhysicalKeyEventPtr event,
                       ProcessKeyEventCallback callback) override;

  // TODO(https://crbug.com/837156): Implement a state for the interface.

 private:
  RuleBasedEngine(const std::string& ime_spec,
                  mojo::PendingReceiver<mojom::InputMethod> receiver,
                  mojo::PendingRemote<mojom::InputMethodHost> host);

  mojo::Receiver<mojom::InputMethod> receiver_;
  mojo::Remote<mojom::InputMethodHost> host_;

  rulebased::Engine engine_;

  // Whether the AltRight key is held down or not.
  bool is_alt_right_key_down_ = false;
};

}  // namespace ime
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_IME_RULE_BASED_ENGINE_H_
