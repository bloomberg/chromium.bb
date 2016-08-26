// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_TEXT_INPUT_CLIENT_IMPL_H_
#define UI_VIEWS_MUS_TEXT_INPUT_CLIENT_IMPL_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/public/interfaces/ime.mojom.h"

namespace ui {
class TextInputClient;
}

namespace views {

class InputMethodMus;

// TextInputClientImpl receieves updates from IME drivers over Mojo IPC, and
// notifies the underlying ui::TextInputClient accordingly.
class TextInputClientImpl : public ui::mojom::TextInputClient {
 public:
  TextInputClientImpl(ui::TextInputClient* text_input_client,
                      InputMethodMus* input_method);
  ~TextInputClientImpl() override;

  ui::mojom::TextInputClientPtr CreateInterfacePtrAndBind();

 private:
  // ui::mojom::TextInputClient:
  void OnCompositionEvent(ui::mojom::CompositionEventPtr event) override;
  void OnUnhandledEvent(std::unique_ptr<ui::Event> key_event) override;

  ui::TextInputClient* text_input_client_;
  InputMethodMus* input_method_;
  mojo::Binding<ui::mojom::TextInputClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(TextInputClientImpl);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_TEXT_INPUT_CLIENT_IMPL_H_
