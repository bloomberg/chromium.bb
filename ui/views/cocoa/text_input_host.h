// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_TEXT_INPUT_HOST_H_
#define UI_VIEWS_COCOA_TEXT_INPUT_HOST_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "ui/views/views_export.h"
#include "ui/views_bridge_mac/mojo/text_input_host.mojom.h"

namespace ui {
class TextInputClient;
}  // namespace ui

namespace views {

class BridgedNativeWidgetHostImpl;

class VIEWS_EXPORT TextInputHost
    : public views_bridge_mac::mojom::TextInputHost {
 public:
  TextInputHost(BridgedNativeWidgetHostImpl* host_impl);
  ~TextInputHost() override;
  void BindRequest(
      views_bridge_mac::mojom::TextInputHostAssociatedRequest request);

  // Set the current TextInputClient.
  void SetTextInputClient(ui::TextInputClient* new_text_input_client);

  // Return a pointer to the host's ui::TextInputClient.
  // TODO(ccameron): Remove the need for this call.
  ui::TextInputClient* GetTextInputClient() const;

 private:
  // views_bridge_mac::mojom::TextInputHost:
  bool HasClient(bool* out_has_client) override;
  bool HasInputContext(bool* out_has_input_context) override;
  bool IsRTL(bool* out_is_rtl) override;
  bool GetSelectionRange(gfx::Range* out_range) override;
  bool GetSelectionText(bool* out_result, base::string16* out_text) override;
  void InsertText(const base::string16& text, bool as_character) override;
  void DeleteRange(const gfx::Range& range) override;
  void SetCompositionText(const base::string16& text,
                          const gfx::Range& selected_range,
                          const gfx::Range& replacement_range) override;
  void ConfirmCompositionText() override;
  bool HasCompositionText(bool* out_has_composition_text) override;
  bool GetCompositionTextRange(gfx::Range* out_composition_range) override;
  bool GetAttributedSubstringForRange(const gfx::Range& requested_range,
                                      base::string16* out_text,
                                      gfx::Range* out_actual_range) override;
  bool GetFirstRectForRange(const gfx::Range& requested_range,
                            gfx::Rect* out_rect,
                            gfx::Range* out_actual_range) override;

  // views_bridge_mac::mojom::TextInputHost synchronous methods:
  void HasClient(HasClientCallback callback) override;
  void HasInputContext(HasInputContextCallback callback) override;
  void IsRTL(IsRTLCallback callback) override;
  void GetSelectionRange(GetSelectionRangeCallback callback) override;
  void GetSelectionText(GetSelectionTextCallback callback) override;
  void HasCompositionText(HasCompositionTextCallback callback) override;
  void GetCompositionTextRange(
      GetCompositionTextRangeCallback callback) override;
  void GetAttributedSubstringForRange(
      const gfx::Range& requested_range,
      GetAttributedSubstringForRangeCallback callback) override;
  void GetFirstRectForRange(const gfx::Range& requested_range,
                            GetFirstRectForRangeCallback callback) override;

  // Weak. If non-null the TextInputClient of the currently focused views::View
  // in the hierarchy rooted at the root view of |host_impl_|. Owned by the
  // focused views::View.
  ui::TextInputClient* text_input_client_ = nullptr;

  // The TextInputClient about to be set. Requests for a new -inputContext will
  // use this, but while the input is changing the NSView still needs to service
  // IME requests using the old |text_input_client_|.
  ui::TextInputClient* pending_text_input_client_ = nullptr;

  BridgedNativeWidgetHostImpl* const host_impl_;

  mojo::AssociatedBinding<views_bridge_mac::mojom::TextInputHost> mojo_binding_;
  DISALLOW_COPY_AND_ASSIGN(TextInputHost);
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_TEXT_INPUT_HOST_H_
