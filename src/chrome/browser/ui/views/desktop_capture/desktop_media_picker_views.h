// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_H_

#include "base/macros.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_controller.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Checkbox;
class TabbedPane;
}  // namespace views

class DesktopMediaPickerViews;

// Dialog view used for DesktopMediaPickerViews.
class DesktopMediaPickerDialogView : public views::DialogDelegateView,
                                     public views::TabbedPaneListener {
 public:
  DesktopMediaPickerDialogView(
      const DesktopMediaPicker::Params& params,
      DesktopMediaPickerViews* parent,
      std::vector<std::unique_ptr<DesktopMediaList>> source_lists);
  ~DesktopMediaPickerDialogView() override;

  // Called by parent (DesktopMediaPickerViews) when it's destroyed.
  void DetachParent();

  // Called by DesktopMediaListController.
  void OnSelectionChanged();
  void AcceptSource();
  void AcceptSpecificSource(content::DesktopMediaID source);
  void OnSourceListLayoutChanged();
  void SelectTab(content::DesktopMediaID::Type source_type);

  // views::TabbedPaneListener:
  void TabSelectedAt(int index) override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  // views::DialogDelegateView:
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  views::View* GetInitiallyFocusedView() override;
  int GetDefaultDialogButton() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  View* CreateExtraView() override;
  bool Accept() override;
  bool ShouldShowCloseButton() const override;
  void DeleteDelegate() override;

 private:
  friend class DesktopMediaPickerViewsTestApi;

  void OnSourceTypeSwitched(int index);

  const DesktopMediaListController* GetSelectedController() const;
  DesktopMediaListController* GetSelectedController();

  DesktopMediaPickerViews* parent_;
  ui::ModalType modality_;

  views::Label* description_label_;

  views::Checkbox* audio_share_checkbox_;

  views::TabbedPane* pane_;
  std::vector<std::unique_ptr<DesktopMediaListController>> list_controllers_;
  std::vector<content::DesktopMediaID::Type> source_types_;

  base::Optional<content::DesktopMediaID> accepted_source_;

  DISALLOW_COPY_AND_ASSIGN(DesktopMediaPickerDialogView);
};

// Implementation of DesktopMediaPicker for Views.
class DesktopMediaPickerViews : public DesktopMediaPicker {
 public:
  DesktopMediaPickerViews();
  ~DesktopMediaPickerViews() override;

  void NotifyDialogResult(content::DesktopMediaID source);

  // DesktopMediaPicker:
  void Show(const DesktopMediaPicker::Params& params,
            std::vector<std::unique_ptr<DesktopMediaList>> source_lists,
            const DoneCallback& done_callback) override;

  DesktopMediaPickerDialogView* GetDialogViewForTesting() const {
    return dialog_;
  }

 private:
  friend class DesktopMediaPickerViewsTestApi;

  DoneCallback callback_;

  // The |dialog_| is owned by the corresponding views::Widget instance.
  // When DesktopMediaPickerViews is destroyed the |dialog_| is destroyed
  // asynchronously by closing the widget.
  DesktopMediaPickerDialogView* dialog_;

  DISALLOW_COPY_AND_ASSIGN(DesktopMediaPickerViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_H_
