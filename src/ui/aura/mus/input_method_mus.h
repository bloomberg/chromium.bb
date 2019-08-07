// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_INPUT_METHOD_MUS_H_
#define UI_AURA_MUS_INPUT_METHOD_MUS_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ws/public/mojom/ime/ime.mojom.h"
#include "ui/aura/aura_export.h"
#include "ui/base/ime/input_method_base.h"

namespace ws {
namespace mojom {
enum class EventResult;
}
}

namespace aura {

class InputMethodMusDelegate;
class InputMethodMusTestApi;
class TextInputClientImpl;

class AURA_EXPORT InputMethodMus : public ui::InputMethodBase,
                                   public ui::AsyncKeyDispatcher {
 public:
  using EventResultCallback = base::OnceCallback<void(ws::mojom::EventResult)>;
  using KeyAckCallback = base::OnceCallback<void(bool)>;

  InputMethodMus(ui::internal::InputMethodDelegate* delegate,
                 InputMethodMusDelegate* input_method_mus_delegate);
  ~InputMethodMus() override;

  void Init(service_manager::Connector* connector);

  // Overridden from ui::AsyncKeyDispatcher:
  void DispatchKeyEventAsync(ui::KeyEvent* event,
                             KeyAckCallback ack_callback) override;

  // Overridden from ui::InputMethod:
  void OnFocus() override;
  void OnBlur() override;
  ui::EventDispatchDetails DispatchKeyEvent(ui::KeyEvent* event) override;
  ui::AsyncKeyDispatcher* GetAsyncKeyDispatcher() override;
  void OnTextInputTypeChanged(const ui::TextInputClient* client) override;
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override;
  void CancelComposition(const ui::TextInputClient* client) override;
  void OnInputLocaleChanged() override;
  bool IsCandidatePopupOpen() const override;
  void ShowVirtualKeyboardIfEnabled() override;

 private:
  friend class InputMethodMusTestApi;
  friend TextInputClientImpl;

  ui::EventDispatchDetails DispatchKeyEventInternal(
      ui::KeyEvent* event,
      EventResultCallback ack_callback) WARN_UNUSED_RESULT;

  // Called from DispatchKeyEvent() to call to the InputMethod.
  ui::EventDispatchDetails SendKeyEventToInputMethod(
      const ui::KeyEvent& event,
      EventResultCallback ack_callback) WARN_UNUSED_RESULT;

  // Overridden from ui::InputMethodBase:
  void OnDidChangeFocusedClient(ui::TextInputClient* focused_before,
                                ui::TextInputClient* focused) override;

  void UpdateTextInputType();

  // Runs all pending callbacks with HANDLED. This is called during shutdown,
  // or any time |input_method_ptr_| is reset to ensure we don't leave mus
  // waiting for an ack. We ack HANDLED because the input method can be reset
  // due to focus changes in response to shortcuts (e.g. Ctrl-T opening a tab)
  // and we don't want the window manager to try to process the accelerators.
  // https://crbug.com/874098
  void AckPendingCallbacks();

  // Called when the server responds to our request to process an event.
  void ProcessKeyEventCallback(
      const ui::KeyEvent& event,
      bool handled);

  // Callback for |text_input_client_| when the input client data may have
  // changed.
  void OnTextInputClientDataChanged(const ui::TextInputClient* client);

  // Delegate used to update window related ime state. This may be null in
  // tests.
  InputMethodMusDelegate* input_method_mus_delegate_;

  // May be null in tests.
  ws::mojom::IMEDriverPtr ime_driver_;
  ws::mojom::InputMethodPtr input_method_ptr_;
  // Typically this is the same as |input_method_ptr_|, but it may be mocked
  // in tests.
  ws::mojom::InputMethod* input_method_ = nullptr;
  std::unique_ptr<TextInputClientImpl> text_input_client_;

  // Callbacks supplied to DispatchKeyEvent() are added here while awaiting
  // the response from the server. These are removed when the response is
  // received (ProcessKeyEventCallback()).
  base::circular_deque<EventResultCallback> pending_callbacks_;

  // Data that was sent to RemoteTextInputClient. Used to suppress sending
  // duplicate data.
  ws::mojom::TextInputClientDataPtr last_sent_text_input_client_data_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodMus);
};

}  // namespace aura

#endif  // UI_AURA_MUS_INPUT_METHOD_MUS_H_
