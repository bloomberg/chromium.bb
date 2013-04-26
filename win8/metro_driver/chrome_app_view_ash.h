// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_METRO_DRIVER_CHROME_APP_VIEW_ASH_H_
#define WIN8_METRO_DRIVER_CHROME_APP_VIEW_ASH_H_

#include <windows.applicationmodel.core.h>
#include <windows.ui.core.h>
#include <windows.ui.input.h>
#include <windows.ui.viewmanagement.h>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "ui/base/events/event_constants.h"
#include "win8/metro_driver/direct3d_helper.h"

namespace IPC {
  class Listener;
  class ChannelProxy;
}

class OpenFilePickerSession;
class SaveFilePickerSession;
class FolderPickerSession;
class FilePickerSessionBase;

struct MetroViewerHostMsg_SaveAsDialogParams;

class ChromeAppViewAsh
    : public mswr::RuntimeClass<winapp::Core::IFrameworkView> {
 public:
  ChromeAppViewAsh();
  ~ChromeAppViewAsh();

  // IViewProvider overrides.
  IFACEMETHOD(Initialize)(winapp::Core::ICoreApplicationView* view);
  IFACEMETHOD(SetWindow)(winui::Core::ICoreWindow* window);
  IFACEMETHOD(Load)(HSTRING entryPoint);
  IFACEMETHOD(Run)();
  IFACEMETHOD(Uninitialize)();

  // Helper function to unsnap the chrome metro app if it is snapped.
  // Returns S_OK on success.
  static HRESULT Unsnap();

  void OnSetCursor(HCURSOR cursor);
  void OnDisplayFileOpenDialog(const string16& title,
                               const string16& filter,
                               const base::FilePath& default_path,
                               bool allow_multiple_files);
  void OnDisplayFileSaveAsDialog(
      const MetroViewerHostMsg_SaveAsDialogParams& params);
  void OnDisplayFolderPicker(const string16& title);

  // This function is invoked when the open file operation completes. The
  // result of the operation is passed in along with the OpenFilePickerSession
  // instance which is deleted after we read the required information from
  // the OpenFilePickerSession class.
  void OnOpenFileCompleted(OpenFilePickerSession* open_file_picker,
                           bool success);

  // This function is invoked when the save file operation completes. The
  // result of the operation is passed in along with the SaveFilePickerSession
  // instance which is deleted after we read the required information from
  // the SaveFilePickerSession class.
  void OnSaveFileCompleted(SaveFilePickerSession* save_file_picker,
                           bool success);

  // This function is invoked when the folder picker operation completes. The
  // result of the operation is passed in along with the FolderPickerSession
  // instance which is deleted after we read the required information from
  // the FolderPickerSession class.
  void OnFolderPickerCompleted(FolderPickerSession* folder_picker,
                               bool success);

 private:
  HRESULT OnActivate(winapp::Core::ICoreApplicationView* view,
                     winapp::Activation::IActivatedEventArgs* args);

  HRESULT OnPointerMoved(winui::Core::ICoreWindow* sender,
                         winui::Core::IPointerEventArgs* args);

  HRESULT OnPointerPressed(winui::Core::ICoreWindow* sender,
                           winui::Core::IPointerEventArgs* args);

  HRESULT OnPointerReleased(winui::Core::ICoreWindow* sender,
                            winui::Core::IPointerEventArgs* args);

  HRESULT OnWheel(winui::Core::ICoreWindow* sender,
                  winui::Core::IPointerEventArgs* args);

  HRESULT OnKeyDown(winui::Core::ICoreWindow* sender,
                    winui::Core::IKeyEventArgs* args);

  HRESULT OnKeyUp(winui::Core::ICoreWindow* sender,
                  winui::Core::IKeyEventArgs* args);

  // Invoked for system keys like Alt, etc.
  HRESULT OnAcceleratorKeyDown(winui::Core::ICoreDispatcher* sender,
                               winui::Core::IAcceleratorKeyEventArgs* args);

  HRESULT OnCharacterReceived(winui::Core::ICoreWindow* sender,
                              winui::Core::ICharacterReceivedEventArgs* args);

  HRESULT OnVisibilityChanged(winui::Core::ICoreWindow* sender,
                              winui::Core::IVisibilityChangedEventArgs* args);

  HRESULT OnWindowActivated(winui::Core::ICoreWindow* sender,
                            winui::Core::IWindowActivatedEventArgs* args);

  mswr::ComPtr<winui::Core::ICoreWindow> window_;
  mswr::ComPtr<winapp::Core::ICoreApplicationView> view_;
  EventRegistrationToken activated_token_;
  EventRegistrationToken pointermoved_token_;
  EventRegistrationToken pointerpressed_token_;
  EventRegistrationToken pointerreleased_token_;
  EventRegistrationToken wheel_token_;
  EventRegistrationToken keydown_token_;
  EventRegistrationToken keyup_token_;
  EventRegistrationToken character_received_token_;
  EventRegistrationToken visibility_changed_token_;
  EventRegistrationToken accel_keydown_token_;
  EventRegistrationToken accel_keyup_token_;
  EventRegistrationToken window_activated_token_;

  // Keep state about which button is currently down, if any, as PointerMoved
  // events do not contain that state, but Ash's MouseEvents need it.
  ui::EventFlags mouse_down_flags_;

  // Set the D3D swap chain and nothing else.
  metro_driver::Direct3DHelper direct3d_helper_;

  // The channel to Chrome, in particular to the MetroViewerProcessHost.
  IPC::ChannelProxy* ui_channel_;

  // The actual window behind the view surface.
  HWND core_window_hwnd_;
};

#endif  // WIN8_METRO_DRIVER_CHROME_APP_VIEW_ASH_H_
