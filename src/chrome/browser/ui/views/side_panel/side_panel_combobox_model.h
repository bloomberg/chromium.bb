// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COMBOBOX_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COMBOBOX_MODEL_H_

#include <string>

#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/image_model.h"

// A model for the side panel entry combobox. This is used to switch
// between available entries within the side panel. The SidePanelCoordinator
// will handle adding and removing side panel entries from this model as their
// availability changes.
class SidePanelComboboxModel : public ui::ComboboxModel {
 public:
  struct Item {
    Item(SidePanelEntry::Id id, std::u16string text, ui::ImageModel icon);
    Item(const Item& other);
    Item& operator=(const Item& other);
    Item(Item&& other);
    Item& operator=(Item&& other);
    ~Item();

    SidePanelEntry::Id id;
    std::u16string text;
    ui::ImageModel icon;
  };

  SidePanelComboboxModel();
  SidePanelComboboxModel(const SidePanelComboboxModel&) = delete;
  SidePanelComboboxModel& operator=(const SidePanelComboboxModel&) = delete;
  ~SidePanelComboboxModel() override;

  void AddItem(SidePanelEntry* entry);
  SidePanelEntry::Id GetIdAt(int index) const;

  // Returns the index for the given side panel entry id, if the id doesn't
  // exist in entries_ then default to 0.
  int GetIndexForId(SidePanelEntry::Id id);

  // ui::ComboboxModel:
  int GetItemCount() const override;
  std::u16string GetItemAt(int index) const override;
  ui::ImageModel GetIconAt(int index) const override;

 private:
  std::vector<Item> entries_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COMBOBOX_MODEL_H_
