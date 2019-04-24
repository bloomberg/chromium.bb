// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_TEXT_INPUT_CLIENT_IMPL_H_
#define UI_AURA_MUS_TEXT_INPUT_CLIENT_IMPL_H_

#include "base/callback.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ws/public/mojom/ime/ime.mojom.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/input_method_delegate.h"

namespace ui {
class TextInputClient;
}

namespace aura {

// TextInputClientImpl receieves updates from IME drivers over Mojo IPC, and
// notifies the underlying ui::TextInputClient accordingly.
class TextInputClientImpl : public ws::mojom::TextInputClient {
 public:
  using OnClientDataChangedCallback =
      base::RepeatingCallback<void(const ui::TextInputClient*)>;

  TextInputClientImpl(ui::TextInputClient* text_input_client,
                      ui::internal::InputMethodDelegate* delegate,
                      OnClientDataChangedCallback on_client_data_changed);
  ~TextInputClientImpl() override;

  ws::mojom::TextInputClientPtr CreateInterfacePtrAndBind();

 private:
  void NotifyClientDataChanged();

  // ws::mojom::TextInputClient:
  void SetCompositionText(const ui::CompositionText& composition) override;
  void ConfirmCompositionText() override;
  void ClearCompositionText() override;
  void InsertText(const base::string16& text) override;
  void InsertChar(std::unique_ptr<ui::Event> event) override;
  void DispatchKeyEventPostIME(
      std::unique_ptr<ui::Event> event,
      DispatchKeyEventPostIMECallback callback) override;
  void EnsureCaretNotInRect(const gfx::Rect& rect) override;
  void SetEditableSelectionRange(const gfx::Range& range) override;
  void DeleteRange(const gfx::Range& range) override;
  void OnInputMethodChanged() override;
  void ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  void ExtendSelectionAndDelete(uint32_t before, uint32_t after) override;

  ui::TextInputClient* text_input_client_;
  mojo::Binding<ws::mojom::TextInputClient> binding_;
  ui::internal::InputMethodDelegate* delegate_;
  OnClientDataChangedCallback on_client_data_changed_;

  DISALLOW_COPY_AND_ASSIGN(TextInputClientImpl);
};

}  // namespace aura

#endif  // UI_AURA_MUS_TEXT_INPUT_CLIENT_IMPL_H_
