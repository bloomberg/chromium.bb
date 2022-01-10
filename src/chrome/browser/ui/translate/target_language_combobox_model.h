// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_TARGET_LANGUAGE_COMBOBOX_MODEL_H_
#define CHROME_BROWSER_UI_TRANSLATE_TARGET_LANGUAGE_COMBOBOX_MODEL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/models/combobox_model.h"

class TranslateBubbleModel;

// The model for the combobox to select a language. This is used for Translate
// user interface to select language.
class TargetLanguageComboboxModel : public ui::ComboboxModel {
 public:
  TargetLanguageComboboxModel(int default_index, TranslateBubbleModel* model);

  TargetLanguageComboboxModel(const TargetLanguageComboboxModel&) = delete;
  TargetLanguageComboboxModel& operator=(const TargetLanguageComboboxModel&) =
      delete;

  ~TargetLanguageComboboxModel() override;

  // Overridden from ui::ComboboxModel:
  int GetItemCount() const override;
  std::u16string GetItemAt(int index) const override;
  int GetDefaultIndex() const override;

 private:
  const int default_index_;
  raw_ptr<TranslateBubbleModel> model_;
};

#endif  // CHROME_BROWSER_UI_TRANSLATE_TARGET_LANGUAGE_COMBOBOX_MODEL_H_
