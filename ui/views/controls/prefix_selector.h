// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_PREFIX_SELECTOR_H_
#define UI_VIEWS_CONTROLS_PREFIX_SELECTOR_H_

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/views/views_export.h"

namespace views {

class PrefixDelegate;

// PrefixSelector is used to change the selection in a view as the user
// types characters.
class VIEWS_EXPORT PrefixSelector : public ui::TextInputClient {
 public:
  explicit PrefixSelector(PrefixDelegate* delegate);
  virtual ~PrefixSelector();

  // Invoked from the view when it loses focus.
  void OnViewBlur();

  // ui::TextInputClient:
  virtual void SetCompositionText(
      const ui::CompositionText& composition) override;
  virtual void ConfirmCompositionText() override;
  virtual void ClearCompositionText() override;
  virtual void InsertText(const base::string16& text) override;
  virtual void InsertChar(base::char16 ch, int flags) override;
  virtual gfx::NativeWindow GetAttachedWindow() const override;
  virtual ui::TextInputType GetTextInputType() const override;
  virtual ui::TextInputMode GetTextInputMode() const override;
  virtual int GetTextInputFlags() const override;
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

 private:
  // Invoked when text is typed. Tries to change the selection appropriately.
  void OnTextInput(const base::string16& text);

  // Returns true if the text of the node at |row| starts with |lower_text|.
  bool TextAtRowMatchesText(int row, const base::string16& lower_text);

  // Clears |current_text_| and resets |time_of_last_key_|.
  void ClearText();

  PrefixDelegate* prefix_delegate_;

  // Time OnTextInput() was last invoked.
  base::TimeTicks time_of_last_key_;

  base::string16 current_text_;

  DISALLOW_COPY_AND_ASSIGN(PrefixSelector);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_PREFIX_SELECTOR_H_
