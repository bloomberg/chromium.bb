// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_PLATFORM_DELEGATE_H_
#define CONTENT_SHELL_BROWSER_SHELL_PLATFORM_DELEGATE_H_

#include "base/containers/flat_map.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_MACOSX)
#include "content/public/browser/native_web_keyboard_event.h"
#endif

class GURL;

namespace content {
class Shell;
class WebContents;

class ShellPlatformDelegate {
 public:
  enum UIControl { BACK_BUTTON, FORWARD_BUTTON, STOP_BUTTON };

  ShellPlatformDelegate();
  virtual ~ShellPlatformDelegate();

  // Helper for one time initialization of application.
  virtual void Initialize(const gfx::Size& default_window_size);

  // Called after creating a Shell instance, with its initial size.
  virtual void CreatePlatformWindow(Shell* shell,
                                    const gfx::Size& initial_size);

  // Called from the Shell destructor to let each platform do any necessary
  // cleanup.
  virtual void CleanUp(Shell* shell);

  // Links the WebContents into the newly created window.
  virtual void SetContents(Shell* shell);

  // Resize the web contents in the shell window to the given size.
  virtual void ResizeWebContent(Shell* shell, const gfx::Size& content_size);

  // Enable/disable a button.
  virtual void EnableUIControl(Shell* shell,
                               UIControl control,
                               bool is_enabled);

  // Updates the url in the url bar.
  virtual void SetAddressBarURL(Shell* shell, const GURL& url);

  // Sets whether the spinner is spinning.
  virtual void SetIsLoading(Shell* shell, bool loading);

  // Set the title of shell window
  virtual void SetTitle(Shell* shell, const base::string16& title);

  // Called when a RenderView is created for a renderer process; forwarded from
  // WebContentsObserver.
  virtual void RenderViewReady(Shell* shell);

  // Destroy the Shell. Returns true if the ShellPlatformDelegate did the
  // destruction. Returns false if the Shell should destroy itself.
  virtual bool DestroyShell(Shell* shell);

#if !defined(OS_ANDROID)
  // Returns the native window. Valid after calling CreatePlatformWindow().
  virtual gfx::NativeWindow GetNativeWindow(Shell* shell);
#endif

#if defined(OS_MACOSX)
  // Activate (make key) the native window, and focus the web contents.
  virtual void ActivateContents(Shell* shell, WebContents* contents);

  // Handle
  virtual bool HandleKeyboardEvent(Shell* shell,
                                   WebContents* source,
                                   const NativeWebKeyboardEvent& event);
#endif

#if defined(OS_ANDROID)
  void ToggleFullscreenModeForTab(Shell* shell,
                                  WebContents* web_contents,
                                  bool enter_fullscreen);

  bool IsFullscreenForTabOrPending(Shell* shell,
                                   const WebContents* web_contents) const;

  // Forwarded from WebContentsDelegate.
  void SetOverlayMode(Shell* shell, bool use_overlay_mode);

  // Forwarded from WebContentsObserver.
  void LoadProgressChanged(Shell* shell, double progress);
#endif

 private:
  // Data held for each Shell instance, since there is one ShellPlatformDelegate
  // for the whole browser process (shared across Shells). This is defined for
  // each platform implementation.
  struct ShellData;

  // Holds an instance of ShellData for each Shell.
  base::flat_map<Shell*, ShellData> shell_data_map_;

  // Data held in ShellPlatformDelegate that is shared between all Shells. This
  // is created in Initialize(), and is defined for each platform
  // implementation.
  struct PlatformData;
  std::unique_ptr<PlatformData> platform_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_PLATFORM_DELEGATE_H_
