// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_ITEM_VIEW_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/flags_ui/feature_entry.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/view.h"

class Browser;
class NewBadgeLabel;
class Profile;
struct LabInfo;

namespace views {
class MdTextButton;
}  // namespace views

class ChromeLabsItemView : public views::View {
 public:
  METADATA_HEADER(ChromeLabsItemView);
  ChromeLabsItemView(
      const LabInfo& lab,
      int default_index,
      const flags_ui::FeatureEntry* feature_entry,
      base::RepeatingCallback<void(ChromeLabsItemView* item_view)>
          combobox_callback,
      Browser* browser);

  int GetSelectedIndex() const;

  views::Combobox* GetLabStateComboboxForTesting() {
    return lab_state_combobox_;
  }

  views::MdTextButton* GetFeedbackButtonForTesting() {
    return feedback_button_;
  }

  NewBadgeLabel* GetNewBadgeForTesting() { return experiment_name_; }

  const flags_ui::FeatureEntry* GetFeatureEntry();

 private:
  bool ShouldShowNewBadge(Profile* profile, const LabInfo& lab);

  raw_ptr<NewBadgeLabel> experiment_name_;

  // Combobox with selected state of the lab.
  views::Combobox* lab_state_combobox_;

  raw_ptr<const flags_ui::FeatureEntry> feature_entry_;

  views::MdTextButton* feedback_button_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_ITEM_VIEW_H_
