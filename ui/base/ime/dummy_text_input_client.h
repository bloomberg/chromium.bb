// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_DUMMY_TEXT_INPUT_CLIENT_H_
#define UI_BASE_IME_DUMMY_TEXT_INPUT_CLIENT_H_

#include "ui/base/ime/text_input_client.h"

namespace ui {

// Dummy implementation of TextInputClient. All functions do nothing.
class DummyTextInputClient : public TextInputClient {
 public:
  DummyTextInputClient();
  explicit DummyTextInputClient(TextInputType text_input_type);
  virtual ~DummyTextInputClient();

  // Overriden from TextInputClient.
  virtual void SetCompositionText(const CompositionText& composition) override;
  virtual void ConfirmCompositionText() override;
  virtual void ClearCompositionText() override;
  virtual void InsertText(const base::string16& text) override;
  virtual void InsertChar(base::char16 ch, int flags) override;
  virtual gfx::NativeWindow GetAttachedWindow() const override;
  virtual TextInputType GetTextInputType() const override;
  virtual TextInputMode GetTextInputMode() const override;
  virtual bool CanComposeInline() const override;
  virtual gfx::Rect GetCaretBounds() const override;
  virtual bool GetCompositionCharacterBounds(uint32 index,
                                             gfx::Rect* rect) const override;
  virtual bool HasCompositionText() const override;
  virtual bool GetTextRange(gfx::Range* range) const override;
  virtual bool GetCompositionTextRange(gfx::Range* range) const override;
  virtual bool GetSelectionRange(gfx::Range* range) const override;
  virtual bool SetSelectionRange(const gfx::Range& range) override;
  virtual bool DeleteRange(const gfx::Range& range) override;
  virtual bool GetTextFromRange(const gfx::Range& range,
                                base::string16* text) const override;
  virtual void OnInputMethodChanged() override;
  virtual bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  virtual void ExtendSelectionAndDelete(size_t before, size_t after) override;
  virtual void EnsureCaretInRect(const gfx::Rect& rect) override;
  virtual void OnCandidateWindowShown() override;
  virtual void OnCandidateWindowUpdated() override;
  virtual void OnCandidateWindowHidden() override;
  virtual bool IsEditingCommandEnabled(int command_id) override;
  virtual void ExecuteEditingCommand(int command_id) override;

  TextInputType text_input_type_;

  DISALLOW_COPY_AND_ASSIGN(DummyTextInputClient);
};

}  // namespace ui

#endif  // UI_BASE_IME_DUMMY_TEXT_INPUT_CLIENT_H_
