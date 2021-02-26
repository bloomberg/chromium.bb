// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_INPUT_ENGINE_H_
#define CHROMEOS_SERVICES_IME_INPUT_ENGINE_H_

#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {
namespace ime {

namespace rulebased {
class Engine;
}

class InputEngineContext {
 public:
  explicit InputEngineContext(const std::string& ime);
  ~InputEngineContext();

  std::string ime_spec;
  std::unique_ptr<rulebased::Engine> engine;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputEngineContext);
};

// A basic implementation of InputEngine without using any decoder.
// TODO(https://crbug.com/1019541): Rename this to RuleBasedEngine.
class InputEngine : public mojom::InputChannel {
 public:
  InputEngine();
  ~InputEngine() override;

  // Binds the mojom::InputChannel interface to this object and returns true if
  // the given ime_spec is supported by the engine.
  virtual bool BindRequest(const std::string& ime_spec,
                           mojo::PendingReceiver<mojom::InputChannel> receiver,
                           mojo::PendingRemote<mojom::InputChannel> remote,
                           const std::vector<uint8_t>& extra);

  // mojom::InputChannel overrides:
  void ProcessMessage(const std::vector<uint8_t>& message,
                      ProcessMessageCallback callback) override;
  void OnInputMethodChanged(const std::string& engine_id) override;
  void OnFocus(mojom::InputFieldInfoPtr input_field_info) override;
  void OnBlur() override;
  void OnSurroundingTextChanged(
      const std::string& text,
      uint32_t offset,
      mojom::SelectionRangePtr selection_range) override;
  void OnCompositionCanceled() override;
  void ProcessKeypressForRulebased(
      mojom::PhysicalKeyEventPtr event,
      ProcessKeypressForRulebasedCallback callback) override;
  void OnKeyEvent(mojom::PhysicalKeyEventPtr event,
                  OnKeyEventCallback callback) override;
  void ResetForRulebased() override;
  void GetRulebasedKeypressCountForTesting(
      GetRulebasedKeypressCountForTestingCallback callback) override;
  void CommitText(const std::string& text) override;
  void SetComposition(const std::string& text) override;
  void SetCompositionRange(uint32_t start_byte_index,
                           uint32_t end_byte_index) override;
  void FinishComposition() override;
  void DeleteSurroundingText(uint32_t num_bytes_before_cursor,
                             uint32_t num_bytes_after_cursor) override;

  // TODO(https://crbug.com/837156): Implement a state for the interface.

 protected:
  // Returns whether the given ime_spec is supported by rulebased engine.
  bool IsImeSupportedByRulebased(const std::string& ime_spec);

 private:
  mojo::ReceiverSet<mojom::InputChannel, std::unique_ptr<InputEngineContext>>
      channel_receivers_;

  // Whether the AltRight key is held down or not. Only used for rule-based.
  bool isAltRightDown_ = false;

  DISALLOW_COPY_AND_ASSIGN(InputEngine);
};

}  // namespace ime
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_IME_INPUT_ENGINE_H_
