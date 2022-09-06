// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_SETTINGS_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_SETTINGS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_menu_group.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Separator;
}  // namespace views

namespace ash {

class CaptureModeBarView;
class CaptureModeMenuGroup;
class CaptureModeSession;

// All the options in the CaptureMode settings view.
enum CaptureSettingsOption {
  kAudioOff = 0,
  kAudioMicrophone,
  kDownloadsFolder,
  kCustomFolder,
  kCameraOff,
  kCameraDevicesBegin,
};

// A view that acts as the content view of the capture mode settings menu
// widget. It is the content view of settings widget and it contains
// `CaptureModeMenuGroup` for each setting, save to, audio input etc.
class ASH_EXPORT CaptureModeSettingsView
    : public views::View,
      public CaptureModeMenuGroup::Delegate,
      public CaptureModeCameraController::Observer {
 public:
  METADATA_HEADER(CaptureModeSettingsView);

  CaptureModeSettingsView(CaptureModeSession* session,
                          bool is_in_projector_mode);
  CaptureModeSettingsView(const CaptureModeSettingsView&) = delete;
  CaptureModeSettingsView& operator=(const CaptureModeSettingsView&) = delete;
  ~CaptureModeSettingsView() override;

  // Gets the ideal bounds in screen coordinates of the settings widget on
  // the given |capture_mode_bar_view|. If |content_view| is not null, it will
  // be used to get the preferred size to calculate the final bounds. Otherwise,
  // a default size will be used.
  static gfx::Rect GetBounds(CaptureModeBarView* capture_mode_bar_view,
                             CaptureModeSettingsView* content_view = nullptr);

  // Called when the folder, in which the captured files will be saved, may have
  // changed. This may result in adding or removing a menu option for the folder
  // that was added or removed. This means that the preferred size of this view
  // can possibly change, and therefore it's the responsibility of the caller to
  // to set the proper bounds on the widget.
  void OnCaptureFolderMayHaveChanged();

  // Called when we change the setting to force-use the default downloads folder
  // as the save folder. This results in updating which folder menu option is
  // currently selected.
  void OnDefaultCaptureFolderSelectionChanged();

  // Gets the highlightable `CaptureModeOption` and `CaptureModeMenuItem` inside
  // this view.
  std::vector<CaptureModeSessionFocusCycler::HighlightableView*>
  GetHighlightableItems();

  // CaptureModeMenuGroup::Delegate:
  void OnOptionSelected(int option_id) const override;
  bool IsOptionChecked(int option_id) const override;
  bool IsOptionEnabled(int option_id) const override;

  // CaptureModeCameraController::Observer:
  void OnAvailableCamerasChanged(const CameraInfoList& cameras) override;
  void OnSelectedCameraChanged(const CameraId& camera_id) override;

  // For tests only:
  CaptureModeMenuGroup* GetAudioInputMenuGroupForTesting() {
    return audio_input_menu_group_;
  }
  views::View* GetMicrophoneOptionForTesting();
  views::View* GetOffOptionForTesting();

 private:
  friend class CaptureModeSettingsTestApi;

  // Called when the "Select folder" menu item in the |save_to_menu_group_| is
  // pressed. It opens the folder selection dialog so that user can pick a
  // location in which captured files will be saved.
  void OnSelectFolderMenuItemPressed();

  // Called back when the check for custom folder's availability is done, with
  // `available` indicating whether the custom folder is available or not. We
  // will check the custom folder's availability every time when
  // `OnCaptureFolderMayHaveChanged` is triggered and custom folder is not
  // empty.
  void OnCustomFolderAvailabilityChecked(bool available);

  // Finds the camera id by the given `option_id` from the
  // `option_camera_id_map_` if any. Otherwise, return nullptr.
  const CameraId* FindCameraIdByOptionId(int option_id) const;

  // Adds all camera options (including the option for `kCameraOff`) for the
  // given `cameras` to the `camera_menu_group_`. It deletes all options in
  // `camera_menu_group_` before adding options. Called when initializing `this`
  // or `OnAvailableCamerasChanged` is triggered. It will also trigger
  // `UpdateCameraMenuGroupVisibility` at the end.
  // Note that camera options are only added when `cameras` is not empty.
  // When cameras are disabled by policy (i.e. `managed_by_policy` is true),
  // only the "Off" option is added. Users are not allowed to choose any cameras
  // in that case.
  void AddCameraOptions(const CameraInfoList& cameras, bool managed_by_policy);

  void UpdateCameraMenuGroupVisibility(bool visible);

  // A reference to the session that owns this view indirectly by owning its
  // containing widget.
  CaptureModeSession* const capture_mode_session_;  // Not null;

  // "Audio input" menu group that users can select an audio input from for
  // screen capture recording. It has "Off" and "Microphone" options for now.
  // "Off" is the default one which means no audio input selected.
  CaptureModeMenuGroup* audio_input_menu_group_ = nullptr;

  // The separator between audio input and camera menus.
  views::Separator* separator_1_ = nullptr;

  // Camera menu group that users can select a camera device from for selfie
  // cam while video recording. It has an `Off` option and options for all
  // available camera devices. `Off` is the default one which means no camera is
  // selected.
  CaptureModeMenuGroup* camera_menu_group_ = nullptr;

  // A mapping from option id to camera id for camera devices.
  base::flat_map<int, CameraId> option_camera_id_map_;

  // Can be null when in Projector mode, since then it's not needed as the
  // "Save-to" menu group will not be added at all.
  views::Separator* separator_2_ = nullptr;

  // "Save to" menu group that users can select a folder to save the captured
  // files to. It will include the "Downloads" folder as the default one and
  // one more folder selected by users.
  // This menu group is not added when in Projector mode, since the folder
  // selection here doesn't affect where Projector saves the videos, and hence
  // it doesn't make sense to show this option. In this case, it remains null.
  CaptureModeMenuGroup* save_to_menu_group_ = nullptr;

  // If not set, custom folder is not set. If true, customer folder is set and
  // available. If false, customer folder is set but unavailable.
  absl::optional<bool> is_custom_folder_available_;

  // If set, it will be called when the settings menu is refreshed.
  base::OnceClosure on_settings_menu_refreshed_callback_for_test_;

  base::WeakPtrFactory<CaptureModeSettingsView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_SETTINGS_VIEW_H_
