// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_TEXT_SUGGESTION_HOST_MOJO_IMPL_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_TEXT_SUGGESTION_HOST_MOJO_IMPL_ANDROID_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/input/input_host.mojom.h"

namespace content {

class TextSuggestionHostAndroid;

// Android implementation of mojom::TextSuggestionHost.
class TextSuggestionHostMojoImplAndroid final
    : public blink::mojom::TextSuggestionHost {
 public:
  TextSuggestionHostMojoImplAndroid(
      TextSuggestionHostAndroid*,
      mojo::PendingReceiver<blink::mojom::TextSuggestionHost> receiver);
  ~TextSuggestionHostMojoImplAndroid() override;

  static std::unique_ptr<TextSuggestionHostMojoImplAndroid> Create(
      TextSuggestionHostAndroid*,
      mojo::PendingReceiver<blink::mojom::TextSuggestionHost> receiver);

  void StartSuggestionMenuTimer() final;

  void ShowSpellCheckSuggestionMenu(
      double caret_x,
      double caret_y,
      const std::string& marked_text,
      std::vector<blink::mojom::SpellCheckSuggestionPtr> suggestions) final;
  void ShowTextSuggestionMenu(
      double caret_x,
      double caret_y,
      const std::string& marked_text,
      std::vector<blink::mojom::TextSuggestionPtr> suggestions) final;

 private:
  TextSuggestionHostAndroid* const text_suggestion_host_;
  mojo::Receiver<blink::mojom::TextSuggestionHost> receiver_;

  DISALLOW_COPY_AND_ASSIGN(TextSuggestionHostMojoImplAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_TEXT_SUGGESTION_HOST_MOJO_IMPL_ANDROID_H_
