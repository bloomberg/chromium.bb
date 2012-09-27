// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Don't include this file from any .h files because it pulls in some X headers.

#ifndef REMOTING_HOST_LINUX_X_SERVER_CLIPBOARD_H_
#define REMOTING_HOST_LINUX_X_SERVER_CLIPBOARD_H_

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/timer.h"

namespace remoting {

// A class to allow manipulation of the X clipboard, using only X API calls.
// This class is not thread-safe, so all its methods must be called on the
// application's main event-processing thread.
class XServerClipboard {
 public:
  typedef base::Callback<void(const std::string& mime_type,
                              const std::string& data)>
    ClipboardChangedCallback;

  XServerClipboard();
  ~XServerClipboard();

  // Start monitoring |display|'s selections, and invoke |callback| whenever
  // their content changes. The caller must ensure |display| is still valid
  // whenever any other methods are called on this object.
  void Init(Display* display, const ClipboardChangedCallback& callback);

  // Copy data to the X Clipboard.  This acquires ownership of the
  // PRIMARY and CLIPBOARD selections.
  void SetClipboard(const std::string& mime_type, const std::string& data);

  // Process |event| if it is an X selection notification. The caller should
  // invoke this for every event it receives from |display|.
  void ProcessXEvent(XEvent* event);

 private:
  // Handlers for X selection events.
  void OnSetSelectionOwnerNotify(Atom selection, Time timestamp);
  void OnPropertyNotify(XEvent* event);
  void OnSelectionNotify(XEvent* event);
  void OnSelectionRequest(XEvent* event);

  // Called when the selection owner has replied to a request for information
  // about a selection.
  void HandleSelectionNotify(XSelectionEvent* event,
                             Atom type,
                             int format,
                             int item_count,
                             void* data);

  // These methods return true if selection processing is complete, false
  // otherwise.
  bool HandleSelectionTargetsEvent(XSelectionEvent* event,
                                   int format,
                                   int item_count,
                                   void* data);
  bool HandleSelectionStringEvent(XSelectionEvent* event,
                                  int format,
                                  int item_count,
                                  void* data);

  // Notify the registered callback of new clipboard text.
  void NotifyClipboardText(const std::string& text);

  // These methods trigger the X server or selection owner to send back an
  // event containing the requested information.
  void RequestSelectionTargets(Atom selection);
  void RequestSelectionString(Atom selection, Atom target);

  // Assert ownership of the specified |selection|.
  void AssertSelectionOwnership(Atom selection);
  bool IsSelectionOwner(Atom selection);

  Display* display_;
  Window clipboard_window_;
  int xfixes_event_base_;
  int xfixes_error_base_;
  Atom clipboard_atom_;
  Atom large_selection_atom_;
  Atom selection_string_atom_;
  Atom targets_atom_;
  Atom timestamp_atom_;
  Atom utf8_string_atom_;
  std::set<Atom> selections_owned_;
  std::string data_;
  Atom large_selection_property_;
  base::TimeTicks get_selections_time_;
  ClipboardChangedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(XServerClipboard);
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_X_SERVER_CLIPBOARD_H_
