// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DEFAULT_PLUGIN_INSTALL_DIALOG_H__
#define WEBKIT_DEFAULT_PLUGIN_INSTALL_DIALOG_H__

#include <atlbase.h>
#include <atlwin.h>
#include <string>
#include <vector>

#include "webkit/default_plugin/default_plugin_resources.h"

class PluginInstallerImpl;

// Displays the plugin installation dialog containing information
// about the mime type of the plugin being downloaded, the URL
// where it would be downloaded from, etc.
class PluginInstallDialog : public CDialogImpl<PluginInstallDialog> {
 public:
  enum {IDD = IDD_DEFAULT_PLUGIN_INSTALL_DIALOG};

  BEGIN_MSG_MAP(PluginInstallDialog)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    COMMAND_ID_HANDLER(IDB_GET_THE_PLUGIN, OnGetPlugin)
    COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
  END_MSG_MAP()

  // Creates or returns the existing object for the given plugin name.
  // Call RemoveInstaller when done.
  static PluginInstallDialog* AddInstaller(PluginInstallerImpl* plugin_impl,
                                           const std::wstring& plugin_name);

  // Lets this object know that the given installer object is going away.
  void RemoveInstaller(PluginInstallerImpl* installer);

  void ShowInstallDialog(HWND parent);

 private:
  PluginInstallDialog(const std::wstring& plugin_name);
  ~PluginInstallDialog();

  // Implemented to ensure that we handle RTL layouts correctly.
  HWND Create(HWND parent_window, LPARAM init_param);

  LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam,
                       BOOL& handled);
  LRESULT OnGetPlugin(WORD notify_code, WORD id, HWND wnd_ctl, BOOL &handled);
  LRESULT OnCancel(WORD notify_code, WORD id, HWND wnd_ctl, BOOL &handled);

  // Wraps the string with Unicode directionality characters in order to make
  // sure BiDi text is rendered correctly when the UI layout is right-to-left.
  void AdjustTextDirectionality(std::wstring* text) const;

  std::vector<PluginInstallerImpl*> installers_;
  std::wstring plugin_name_;
};

#endif  // WEBKIT_DEFAULT_PLUGIN_INSTALL_DIALOG_H__
