// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_WM_IPC_ENUMS_H_
#define CHROMEOS_WM_IPC_ENUMS_H_

// This file defines enums that are shared between Chrome and the Chrome OS
// window manager.

namespace chromeos {

// A window's _CHROME_WINDOW_TYPE property contains a sequence of 32-bit
// integer values that inform the window manager how the window should be
// treated.  This enum lists the possible values for the first element in a
// property.  If other elements are required after the window type, they
// are listed here; for example, param[0] indicates the second value that
// should appear in the property, param[1] the third, and so on.
//
// Do not re-use values -- this list is shared between multiple processes.
enum WmIpcWindowType {
  // A non-Chrome window, or one that doesn't need to be handled in any
  // special way by the window manager.
  WM_IPC_WINDOW_UNKNOWN = 0,

  // A top-level Chrome window.
  //   param[0]: The number of tabs currently in this Chrome window.
  //   param[1]: The index of the currently selected tab in this
  //             Chrome window.
  WM_IPC_WINDOW_CHROME_TOPLEVEL = 1,

  // The contents of a popup window, displayed as a panel.
  //   param[0]: X ID of associated titlebar, which must be mapped before
  //             its content.
  //   param[1]: Initial state for panel (0 is collapsed, 1 is expanded).
  //   param[2]: Should the panel be initially focused? (0 is no, 1 is yes).
  //   param[3]: Panel's creator.  If this contains the X ID of another
  //             panel's content window, this panel will be opened to the
  //             immediate left of it.
  //   param[4]: How the user should be able to resize the panel.  See
  //             the WmIpcPanelUserResizeType enum; defaults to allowing
  //             both horizontal and vertical resizing.
  WM_IPC_WINDOW_CHROME_PANEL_CONTENT = 4,

  // A small window placed above the panel's contents containing its title
  // and a close button.
  WM_IPC_WINDOW_CHROME_PANEL_TITLEBAR = 5,

  // A Chrome info bubble (e.g. the bookmark bubble).  These are
  // transient RGBA windows; we skip the usual transient behavior of
  // centering them over their owner and omit drawing a drop shadow.
  //   param[0]: Should the window manager composite the window even while the
  //             screen is locked?  Non-zero means "yes".  This is used for e.g.
  //             the volume bubble and is only honored for override-redirect
  //             windows.
  WM_IPC_WINDOW_CHROME_INFO_BUBBLE = 7,

  // A window showing a view of a tab within a Chrome window.
  //   param[0]: X ID of toplevel window that owns it.
  //   param[1]: Index of this tab in the toplevel window that owns it.
  WM_IPC_WINDOW_CHROME_TAB_SNAPSHOT = 8,

  // The following types are used for the windows that represent a user that
  // has already logged into the system.
  //
  // Visually the BORDER contains the IMAGE and CONTROLS windows, the LABEL
  // and UNSELECTED_LABEL are placed beneath the BORDER. The LABEL window is
  // onscreen when the user is selected, otherwise the UNSELECTED_LABEL is
  // on screen.
  //
  // The following parameters are set for these windows (except BACKGROUND):
  //   param[0]: Visual index of the user the window corresponds to.
  //             For example, all windows with an index of 0 occur first,
  //             followed by windows with an index of 1...
  //
  // The following additional params are set on all BORDER windows:
  //   param[1]: Total number of users.
  //   param[2]: Size of the unselected image.
  //   param[3]: Gap between image and controls.
  //
  // The following param is set on the BACKGROUND window:
  //   param[0]: Whether Chrome has finished painting the background
  //             (1 means "yes").
  WM_IPC_WINDOW_LOGIN_BORDER = 9,
  WM_IPC_WINDOW_LOGIN_IMAGE = 10,
  WM_IPC_WINDOW_LOGIN_CONTROLS = 11,
  WM_IPC_WINDOW_LOGIN_LABEL = 12,
  WM_IPC_WINDOW_LOGIN_UNSELECTED_LABEL = 13,
  WM_IPC_WINDOW_LOGIN_BACKGROUND = 15,

  // The GUEST window is used for all OOBE windows and other
  // wizard windows such as "Create account".
  // param[0]: Non-zero value represents "initial show".
  // That means that screen is created for the first time
  // (device is booted). Both screen and background are animated.
  // Zero value represents that screen is recreated but
  // background animation is not needed.
  // This parameter is only checked when window is mapped.
  WM_IPC_WINDOW_LOGIN_GUEST = 14,

  // A window that Chrome opens when locking the screen.
  WM_IPC_WINDOW_CHROME_SCREEN_LOCKER = 16,

  // A window showing the title of a tab for use in overview mode.
  //   param[0]: X ID of snapshot window that is associated with this title.
  WM_IPC_WINDOW_CHROME_TAB_TITLE = 17,

  // A window showing the fav icon of a tab for use in overview mode.
  //   param[0]: X ID of snapshot window that is associated with this fav icon.
  WM_IPC_WINDOW_CHROME_TAB_FAV_ICON = 18,

  // A WebUI Browser for displaying startup screens.
  WM_IPC_WINDOW_LOGIN_WEBUI = 19,

  // NEXT VALUE TO USE: 20
};

inline bool WmIpcWindowTypeIsChrome(WmIpcWindowType type) {
  return type != WM_IPC_WINDOW_UNKNOWN;
}

inline const char* WmIpcWindowTypeToString(WmIpcWindowType type) {
  switch (type) {
    case WM_IPC_WINDOW_UNKNOWN:                return "UNKNOWN";
    case WM_IPC_WINDOW_CHROME_TOPLEVEL:        return "CHROME_TOPLEVEL";
    case WM_IPC_WINDOW_CHROME_PANEL_CONTENT:   return "CHROME_PANEL_CONTENT";
    case WM_IPC_WINDOW_CHROME_PANEL_TITLEBAR:  return "CHROME_PANEL_TITLEBAR";
    case WM_IPC_WINDOW_CHROME_INFO_BUBBLE:     return "CHROME_INFO_BUBBLE";
    case WM_IPC_WINDOW_CHROME_TAB_SNAPSHOT:    return "CHROME_TAB_SNAPSHOT";
    case WM_IPC_WINDOW_LOGIN_BORDER:           return "LOGIN_BORDER";
    case WM_IPC_WINDOW_LOGIN_IMAGE:            return "LOGIN_IMAGE";
    case WM_IPC_WINDOW_LOGIN_CONTROLS:         return "LOGIN_CONTROLS";
    case WM_IPC_WINDOW_LOGIN_LABEL:            return "LOGIN_LABEL";
    case WM_IPC_WINDOW_LOGIN_UNSELECTED_LABEL: return "LOGIN_UNSELECTED_LABEL";
    case WM_IPC_WINDOW_LOGIN_GUEST:            return "LOGIN_GUEST";
    case WM_IPC_WINDOW_LOGIN_BACKGROUND:       return "LOGIN_BACKGROUND";
    case WM_IPC_WINDOW_CHROME_SCREEN_LOCKER:   return "CHROME_SCREEN_LOCKER";
    case WM_IPC_WINDOW_CHROME_TAB_TITLE:       return "CHROME_TAB_TITLE";
    case WM_IPC_WINDOW_CHROME_TAB_FAV_ICON:    return "CHROME_TAB_FAV_ICON";
    case WM_IPC_WINDOW_LOGIN_WEBUI:            return "LOGIN_WEBUI";
    default:                                   return "INVALID";
  }
}

// Messages are sent via ClientMessage events that have 'message_type' set
// to _CHROME_WM_MESSAGE, 'format' set to 32 (that is, 32-bit values), and
// l[0] set to a value from the WmIpcMessageType enum.  The remaining four
// values in the 'l' array contain data appropriate to the type of message
// being sent.
//
// Message names should take the form:
//   WM_IPC_MESSAGE_<recipient>_<description>
//
// Do not re-use values -- this list is shared between multiple processes.
enum WmIpcMessageType {
  WM_IPC_MESSAGE_UNKNOWN = 0,

  // Tell the WM to collapse or expand a panel.
  //   param[0]: X ID of the panel window.
  //   param[1]: Desired state (0 means collapsed, 1 means expanded).
  WM_IPC_MESSAGE_WM_SET_PANEL_STATE = 4,

  // Notify Chrome that the panel state has changed.  Sent to the panel
  // window.
  //   param[0]: new state (0 means collapsed, 1 means expanded).
  // TODO: Deprecate this; Chrome can just watch for changes to the
  // _CHROME_STATE property to get the same information.
  WM_IPC_MESSAGE_CHROME_NOTIFY_PANEL_STATE = 5,

  // Notify the WM that a panel has been dragged.
  //   param[0]: X ID of the panel's content window.
  //   param[1]: X coordinate to which the upper-right corner of the
  //             panel's titlebar window was dragged.
  //   param[2]: Y coordinate to which the upper-right corner of the
  //             panel's titlebar window was dragged.
  // Note: The point given is actually that of one pixel to the right
  // of the upper-right corner of the titlebar window.  For example, a
  // no-op move message for a 10-pixel wide titlebar whose upper-left
  // point is at (0, 0) would contain the X and Y paremeters (10, 0):
  // in other words, the position of the titlebar's upper-left point
  // plus its width.  This is intended to make both the Chrome and WM
  // side of things simpler and to avoid some easy-to-make off-by-one
  // errors.
  WM_IPC_MESSAGE_WM_NOTIFY_PANEL_DRAGGED = 7,

  // Notify the WM that the panel drag is complete (that is, the mouse
  // button has been released).
  //   param[0]: X ID of the panel's content window.
  WM_IPC_MESSAGE_WM_NOTIFY_PANEL_DRAG_COMPLETE = 8,

  // Notify Chrome that the layout mode (for example, overview or
  // active) has changed.
  //   param[0]: New mode (0 means active, 1 means overview).
  //   param[1]: Was the mode cancelled? (0 means no, 1 means yes).
  WM_IPC_MESSAGE_CHROME_NOTIFY_LAYOUT_MODE = 10,

  // Let the WM know which version of this file Chrome is using.  It's
  // difficult to make changes synchronously to Chrome and the WM (our
  // build scripts can use a locally-built Chromium, the latest one
  // from the buildbot, or an older hardcoded version), so it's useful
  // to be able to maintain compatibility in the WM with versions of
  // Chrome that exhibit older behavior.
  //
  // Chrome should send a message to the WM at (the WM's) startup
  // containing the version number from the list below describing the
  // behavior that it implements.  For backwards compatibility, the WM
  // assumes version 1 if it doesn't receive a message.  Here are the
  // changes that have been made in successive versions of the protocol:
  //
  // 1: WM_NOTIFY_PANEL_DRAGGED contains the position of the
  //    upper-right, rather than upper-left, corner of of the titlebar
  //    window
  //
  //   param[0]: Version of this protocol currently supported by Chrome.
  WM_IPC_MESSAGE_WM_NOTIFY_IPC_VERSION = 12,

  // Notify Chrome when a tab has been selected in the overview.  Sent to the
  // toplevel window associated with the magnified tab.
  //   param[0]: Tab index of the newly-selected tab.
  WM_IPC_MESSAGE_CHROME_NOTIFY_TAB_SELECT = 13,

  // Tell the window manager to hide the login windows.
  WM_IPC_MESSAGE_WM_HIDE_LOGIN = 14,

  // Set whether login is enabled.  If true, the user can click on any of the
  // login windows to select one, if false clicks on unselected windows are
  // ignored.  This is used when the user attempts a login to make sure the
  // user doesn't select another user.
  //   param[0]: True to enable, false to disable.
  WM_IPC_MESSAGE_WM_SET_LOGIN_STATE = 15,

  // Notify chrome when the guest entry is selected and the guest window
  // hasn't been created yet.
  WM_IPC_MESSAGE_CHROME_CREATE_GUEST_WINDOW = 16,

  // Notify Chrome when a system key of interest is clicked, so volume up/down
  // and mute can be handled (chrome can add visual feedback).  This message
  // could be extended for other special purpose keys (maybe multimedia keys
  // like play/pause/ff/rr).
  //   param[0]: Which key was pressed, from WmIpcSystemKey enum.
  //
  // TODO(davej): Eventually this message should be deprecated in favor of
  // Chrome handling these sorts of keypresses internally.
  WM_IPC_MESSAGE_CHROME_NOTIFY_SYSKEY_PRESSED = 17,

  // Tell the window manager to select a user on the login screen.
  //   param[0]: 0-based index of entry to be selected.
  WM_IPC_MESSAGE_WM_SELECT_LOGIN_USER = 18,

  // Notify Chrome that the screen has been redrawn in response to a screen
  // locker window having been mapped.  We use this to ensure that on
  // suspend, the window manager gets a chance to draw the locked
  // environment before the system is suspended -- otherwise, the
  // pre-locked environment may be briefly visible on resume.  The message
  // will be sent to the Chrome screen locker window that triggered the
  // lock.
  WM_IPC_MESSAGE_CHROME_NOTIFY_SCREEN_REDRAWN_FOR_LOCK = 19,

  // Sent from the power manager to the window manager when we're shutting down.
  WM_IPC_MESSAGE_WM_NOTIFY_SHUTTING_DOWN = 20,

  // Sent from the power manager to the window manager when we change state
  // as a result of the power button being held down.
  //   param[0]: New state, from WmIpcPowerButtonState enum.
  WM_IPC_MESSAGE_WM_NOTIFY_POWER_BUTTON_STATE = 21,

  // Sent from Chrome to the window manager when the user is signing out.
  WM_IPC_MESSAGE_WM_NOTIFY_SIGNING_OUT = 22,

  // Sent from Chrome to the window manager to tell it to switch to
  // the next or previous window.  Wraps around if already on "last"
  // window.
  //   param[0]: true (1) if switching to the next window, false (0)
  //             if switching to the previous window.
  WM_IPC_MESSAGE_WM_CYCLE_WINDOWS = 23,

  // Sent from Chrome to the window manager to ask it to switch to a particular
  // layout mode (e.g. in response to a maximize/restore button being clicked).
  // The current state is stored in a _CHROME_LAYOUT_MODE property on the root
  // window by the window manager.
  //   param[0]: New mode, from WmIpcLayoutMode.
  WM_IPC_MESSAGE_WM_SET_LAYOUT_MODE = 24,

  // NEXT VALUE TO USE: 25
};

inline const char* WmIpcMessageTypeToString(WmIpcMessageType type) {
  switch (type) {
    case WM_IPC_MESSAGE_UNKNOWN:
      return "UNKNOWN";
    case WM_IPC_MESSAGE_WM_SET_PANEL_STATE:
      return "WM_SET_PANEL_STATE";
    case WM_IPC_MESSAGE_CHROME_NOTIFY_PANEL_STATE:
      return "CHROME_NOTIFY_PANEL_STATE";
    case WM_IPC_MESSAGE_WM_NOTIFY_PANEL_DRAGGED:
      return "WM_NOTIFY_PANEL_DRAGGED";
    case WM_IPC_MESSAGE_WM_NOTIFY_PANEL_DRAG_COMPLETE:
      return "WM_NOTIFY_PANEL_DRAG_COMPLETE";
    case WM_IPC_MESSAGE_CHROME_NOTIFY_LAYOUT_MODE:
      return "CHROME_NOTIFY_LAYOUT_MODE";
    case WM_IPC_MESSAGE_WM_NOTIFY_IPC_VERSION:
      return "WM_NOTIFY_IPC_VERSION";
    case WM_IPC_MESSAGE_CHROME_NOTIFY_TAB_SELECT:
      return "CHROME_NOTIFY_TAB_SELECT";
    case WM_IPC_MESSAGE_WM_HIDE_LOGIN:
      return "WM_HIDE_LOGIN";
    case WM_IPC_MESSAGE_WM_SET_LOGIN_STATE:
      return "WM_SET_LOGIN_STATE";
    case WM_IPC_MESSAGE_CHROME_CREATE_GUEST_WINDOW:
      return "CHROME_CREATE_GUEST_WINDOW";
    case WM_IPC_MESSAGE_CHROME_NOTIFY_SYSKEY_PRESSED:
      return "CHROME_NOTIFY_SYSKEY_PRESSED";
    case WM_IPC_MESSAGE_WM_SELECT_LOGIN_USER:
      return "WM_SELECT_LOGIN_USER";
    case WM_IPC_MESSAGE_CHROME_NOTIFY_SCREEN_REDRAWN_FOR_LOCK:
      return "CHROME_NOTIFY_SCREEN_REDRAWN_FOR_LOCK";
    case WM_IPC_MESSAGE_WM_NOTIFY_SHUTTING_DOWN:
      return "WM_NOTIFY_SHUTTING_DOWN";
    case WM_IPC_MESSAGE_WM_NOTIFY_POWER_BUTTON_STATE:
      return "WM_NOTIFY_POWER_BUTTON_STATE";
    case WM_IPC_MESSAGE_WM_NOTIFY_SIGNING_OUT:
      return "WM_NOTIFY_SIGNING_OUT";
    case WM_IPC_MESSAGE_WM_CYCLE_WINDOWS:
      return "WM_CYCLE_WINDOWS";
    case WM_IPC_MESSAGE_WM_SET_LAYOUT_MODE:
      return "WM_SET_LAYOUT_MODE";
    default:
      return "INVALID";
  }
}

// A parameter of the WM_IPC_MESSAGE_CHROME_NOTIFY_SYSKEY_PRESSED message
// denoting which key is pressed.
enum WmIpcSystemKey {
  WM_IPC_SYSTEM_KEY_VOLUME_MUTE = 0,
  WM_IPC_SYSTEM_KEY_VOLUME_DOWN = 1,
  WM_IPC_SYSTEM_KEY_VOLUME_UP = 2
};

// A parameter set on WM_IPC_WINDOW_CHROME_PANEL_CONTENT to describe how
// the user should be allowed to resize a panel, if at all.
enum WmIpcPanelUserResizeType {
  WM_IPC_PANEL_USER_RESIZE_HORIZONTALLY_AND_VERTICALLY = 0,
  WM_IPC_PANEL_USER_RESIZE_HORIZONTALLY,
  WM_IPC_PANEL_USER_RESIZE_VERTICALLY,
  WM_IPC_PANEL_USER_RESIZE_NONE,
};

// A parameter used by WM_IPC_MESSAGE_WM_NOTIFY_POWER_BUTTON_STATE to
// describe the state to which we've transitioned.
enum WmIpcPowerButtonState {
  // The power button has just been pressed while in the unlocked state.
  WM_IPC_POWER_BUTTON_PRE_LOCK = 0,

  // The power button was released before being held long enough to lock
  // the screen.
  WM_IPC_POWER_BUTTON_ABORTED_LOCK,

  // The power button has just been pressed while in the locked or
  // not-logged-in state, or it's been held throughout the locked state
  // long enough that we should let the user know that we're about to shut
  // down.
  WM_IPC_POWER_BUTTON_PRE_SHUTDOWN,

  // The power button was released before being held long enough to shut
  // down the machine.
  WM_IPC_POWER_BUTTON_ABORTED_SHUTDOWN,
};

// A parameter used by WM_IPC_MESSAGE_WM_SET_LAYOUT_MODE to describe the
// requested window layout mode.
enum WmIpcLayoutMode {
  // The active browser window is covering the whole screen.
  WM_IPC_LAYOUT_MAXIMIZED = 0,

  // Multiple browser windows are visible onscreen at once and are overlapping
  // each other.
  WM_IPC_LAYOUT_OVERLAPPING,
};

}  // namespace chromeos

#endif  // CHROMEOS_WM_IPC_ENUMS_H_
