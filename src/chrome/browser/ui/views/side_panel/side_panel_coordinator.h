// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry_observer.h"

class Browser;
class BrowserView;
class SidePanelComboboxModel;

namespace views {
class Combobox;
class View;
}  // namespace views

// Class used to manage the state of side-panel content. Clients should manage
// side-panel visibility using this class rather than explicitly showing/hiding
// the side-panel View.
// This class is also responsible for consolidating multiple SidePanelEntry
// classes across multiple SidePanelRegistry instances, potentially merging them
// into a single unified side panel.
class SidePanelCoordinator final : public SidePanelRegistryObserver {
 public:
  explicit SidePanelCoordinator(BrowserView* browser_view,
                                SidePanelRegistry* global_registry);
  SidePanelCoordinator(const SidePanelCoordinator&) = delete;
  SidePanelCoordinator& operator=(const SidePanelCoordinator&) = delete;
  ~SidePanelCoordinator() override;

  void Show(absl::optional<SidePanelEntry::Id> entry_id = absl::nullopt);
  void Close();
  void Toggle();

 private:
  views::View* GetContentView();
  SidePanelEntry* GetEntryForId(SidePanelEntry::Id entry_id);

  // Creates header and SidePanelEntry content container within the side panel.
  void InitializeSidePanel();

  // Removes existing SidePanelEntry contents from the side panel if any exist
  // and populates the side panel with the provided SidePanelEntry.
  void PopulateSidePanel(SidePanelEntry* entry);

  // Returns the last active entry or the reading list entry if no last active
  // entry exists.
  SidePanelEntry::Id GetLastActiveEntry() const;

  std::unique_ptr<views::View> CreateHeader();
  std::unique_ptr<views::Combobox> CreateCombobox();
  void OnComboboxChanged();

  std::unique_ptr<views::View> CreateBookmarksWebView(Browser* browser);
  std::unique_ptr<views::View> CreateReaderModeWebView(Browser* browser);

  // SidePanelRegistryObserver:
  void OnEntryRegistered(SidePanelEntry* entry) override;

  const raw_ptr<BrowserView> browser_view_;
  raw_ptr<SidePanelRegistry> global_registry_;

  // Used to update SidePanelEntry options in the header_combobox_ based on
  // their availability in the observed side panel registries.
  std::unique_ptr<SidePanelComboboxModel> combobox_model_;
  raw_ptr<views::Combobox> header_combobox_ = nullptr;

  // TODO(pbos): Add awareness of tab registries here. This probably needs to
  // know the tab registry it's currently monitoring.
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_
