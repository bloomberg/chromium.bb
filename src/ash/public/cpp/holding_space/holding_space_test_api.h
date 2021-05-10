// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_TEST_API_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_TEST_API_H_

#include <vector>

#include "ash/ash_export.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class View;
}  // namespace views

namespace ash {

class HoldingSpaceTray;

// Utility class to facilitate easier testing of holding space. This API mainly
// exists to workaround //ash dependency restrictions in browser tests.
class ASH_EXPORT HoldingSpaceTestApi {
 public:
  HoldingSpaceTestApi();
  HoldingSpaceTestApi(const HoldingSpaceTestApi&) = delete;
  HoldingSpaceTestApi& operator=(const HoldingSpaceTestApi&) = delete;
  ~HoldingSpaceTestApi();

  // Returns the root window that newly created windows should be added to.
  static aura::Window* GetRootWindowForNewWindows();

  // Shows holding space UI. This is a no-op if it's already showing.
  void Show();

  // Closes holding space UI. This is a no-op if it's already closed.
  void Close();

  // Returns true if holding space UI is showing, false otherwise.
  bool IsShowing();

  // Returns true if the holding space tray is showing in the shelf, false
  // otherwise.
  bool IsShowingInShelf();

  // Returns the header of the downloads section in holding space UI.
  views::View* GetDownloadsSectionHeader();

  // Returns the collection of download chips in holding space UI.
  // If holding space UI is not visible, an empty collection is returned.
  std::vector<views::View*> GetDownloadChips();

  // Returns the collection of pinned file chips in holding space UI.
  // If holding space UI is not visible, an empty collection is returned.
  std::vector<views::View*> GetPinnedFileChips();

  // Returns the collection of screen capture views in holding space UI.
  // If holding space UI is not visible, an empty collection is returned.
  std::vector<views::View*> GetScreenCaptureViews();

  // Returns the holding space tray in the shelf.
  views::View* GetTray();

  // Returns the view drawn on top of the holding space tray to indicate that
  // it is a drop target capable of handling the current drag payload.
  views::View* GetTrayDropTargetOverlay();

  // Returns the holding space tray icon view for the default, non content
  // forward  icon.
  views::View* GetDefaultTrayIcon();

  // Returns the holding space tray icon view for the content forward icon,
  // which displays previews of most recent items added to holding space.
  views::View* GetPreviewsTrayIcon();

  // Returns the pinned files bubble.
  views::View* GetPinnedFilesBubble();

  // Returns whether the pinned files bubble is shown.
  bool PinnedFilesBubbleShown() const;

  // Returns whether the recent files bubble is shown.
  bool RecentFilesBubbleShown() const;

 private:
  HoldingSpaceTray* holding_space_tray_ = nullptr;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_TEST_API_H_
