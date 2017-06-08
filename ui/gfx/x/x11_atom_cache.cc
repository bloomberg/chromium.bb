// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/x11_atom_cache.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <memory>

#include "base/logging.h"
#include "base/memory/singleton.h"

namespace {

const char* kAtomsToCache[] = {"Abs Dbl End Timestamp",
                               "Abs Dbl Fling X Velocity",
                               "Abs Dbl Fling Y Velocity",
                               "Abs Dbl Metrics Data 1",
                               "Abs Dbl Metrics Data 2",
                               "Abs Dbl Ordinal X",
                               "Abs Dbl Ordinal Y",
                               "Abs Dbl Start Timestamp",
                               "Abs Finger Count",
                               "Abs Fling State",
                               "Abs Metrics Type",
                               "Abs MT Orientation",
                               "Abs MT Position X",
                               "Abs MT Position Y",
                               "Abs MT Pressure",
                               "Abs MT Touch Major",
                               "Abs MT Touch Minor",
                               "Abs MT Tracking ID",
                               "application/octet-stream",
                               "application/vnd.chromium.test",
                               "ATOM",
                               "ATOM_PAIR",
                               "CARDINAL",
                               "CHECK",
                               "_CHROME_DISPLAY_INTERNAL",
                               "_CHROME_DISPLAY_ROTATION",
                               "_CHROME_DISPLAY_SCALE_FACTOR",
                               "CHOME_SELECTION",
                               "CHROME_SELECTION",
                               "_CHROMIUM_DRAG_RECEIVER",
                               "chromium/filename",
                               "CHROMIUM_TIMESTAMP",
                               "chromium/x-bookmark-entries",
                               "chromium/x-browser-actions",
                               "chromium/x-file-system-files",
                               "chromium/x-pepper-custom-data",
                               "chromium/x-renderer-taint",
                               "chromium/x-web-custom-data",
                               "chromium/x-webkit-paste",
                               "CLIPBOARD",
                               "CLIPBOARD_MANAGER",
                               "Content Protection",
                               "Desired",
                               "Device Node",
                               "Device Product ID",
                               "EDID",
                               "Enabled",
                               "FAKE_SELECTION",
                               "Full aspect",
                               "_GTK_HIDE_TITLEBAR_WHEN_MAXIMIZED",
                               "_ICC_PROFILE",
                               "image/png",
                               "INCR",
                               "KEYBOARD",
                               "LOCK",
                               "marker_event",
                               "_MOTIF_WM_HINTS",
                               "MOUSE",
                               "MULTIPLE",
                               "_NET_ACTIVE_WINDOW",
                               "_NET_CLIENT_LIST_STACKING",
                               "_NET_CURRENT_DESKTOP",
                               "_NET_FRAME_EXTENTS",
                               "_NETSCAPE_URL",
                               "_NET_SUPPORTED",
                               "_NET_SUPPORTING_WM_CHECK",
                               "_NET_WM_CM_S0",
                               "_NET_WM_DESKTOP",
                               "_NET_WM_ICON",
                               "_NET_WM_MOVERESIZE",
                               "_NET_WM_NAME",
                               "_NET_WM_PID",
                               "_NET_WM_PING",
                               "_NET_WM_STATE",
                               "_NET_WM_STATE_ABOVE",
                               "_NET_WM_STATE_FOCUSED",
                               "_NET_WM_STATE_FULLSCREEN",
                               "_NET_WM_STATE_HIDDEN",
                               "_NET_WM_STATE_MAXIMIZED_HORZ",
                               "_NET_WM_STATE_MAXIMIZED_VERT",
                               "_NET_WM_STATE_SKIP_TASKBAR",
                               "_NET_WM_STATE_STICKY",
                               "_NET_WM_USER_TIME",
                               "_NET_WM_WINDOW_OPACITY",
                               "_NET_WM_WINDOW_TYPE",
                               "_NET_WM_WINDOW_TYPE_DND",
                               "_NET_WM_WINDOW_TYPE_MENU",
                               "_NET_WM_WINDOW_TYPE_NORMAL",
                               "_NET_WM_WINDOW_TYPE_NOTIFICATION",
                               "_NET_WM_WINDOW_TYPE_TOOLTIP",
                               "_NET_WORKAREA",
                               "Rel Horiz Wheel",
                               "Rel Vert Wheel",
                               "SAVE_TARGETS",
                               "_SCREENSAVER_STATUS",
                               "_SCREENSAVER_VERSION",
                               "scaling mode",
                               "SELECTION_STRING",
                               "STRING",
                               "Tap Paused",
                               "TARGETS",
                               "TARGET1",
                               "TARGET2",
                               "TEXT",
                               "text/html",
                               "text/plain",
                               "text/plain;charset=utf-8",
                               "text/uri-list",
                               "text/rtf",
                               "text/x-moz-url",
                               "TIMESTAMP",
                               "TOUCHPAD",
                               "TOUCHSCREEN",
                               "Touch Timestamp",
                               "Undesired",
                               "UTF8_STRING",
                               "WM_CLASS",
                               "WM_DELETE_WINDOW",
                               "WM_PROTOCOLS",
                               "WM_WINDOW_ROLE",
                               "XdndActionAsk",
                               "XdndActionCopy",
                               "XdndActionDirectSave",
                               "XdndActionLink",
                               "XdndActionList",
                               "XdndActionMove",
                               "XdndActionPrivate",
                               "XdndAware",
                               "XdndDirectSave0",
                               "XdndDrop",
                               "XdndEnter",
                               "XdndFinished",
                               "XdndLeave",
                               "XdndPosition",
                               "XdndProxy",
                               "XdndSelection",
                               "XdndStatus",
                               "XdndTypeList",
                               nullptr};

}  // namespace

namespace gfx {

XAtom GetAtom(const char* name) {
  return X11AtomCache::GetInstance()->GetAtom(name);
}

X11AtomCache* X11AtomCache::GetInstance() {
  return base::Singleton<X11AtomCache>::get();
}

X11AtomCache::X11AtomCache() : xdisplay_(gfx::GetXDisplay()) {
  int cache_count = 0;
  for (const char* const* i = kAtomsToCache; *i; ++i)
    ++cache_count;

  std::unique_ptr<XAtom[]> cached_atoms(new XAtom[cache_count]);

  // Grab all the atoms we need now to minimize roundtrips to the X11 server.
  XInternAtoms(xdisplay_, const_cast<char**>(kAtomsToCache), cache_count, False,
               cached_atoms.get());

  for (int i = 0; i < cache_count; ++i)
    cached_atoms_.insert(std::make_pair(kAtomsToCache[i], cached_atoms[i]));
}

X11AtomCache::~X11AtomCache() {}

XAtom X11AtomCache::GetAtom(const char* name) const {
  std::map<std::string, Atom>::const_iterator it = cached_atoms_.find(name);

  if (it == cached_atoms_.end()) {
    XAtom atom = XInternAtom(xdisplay_, name, false);
    cached_atoms_.insert(std::make_pair(name, atom));
    return atom;
  }

  return it->second;
}

}  // namespace gfx
