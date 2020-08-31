// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data_provider_x11.h"

#include <utility>

#include "base/check.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/x/selection_utils.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/x11_atom_cache.h"

namespace ui {

OSExchangeDataProviderX11::OSExchangeDataProviderX11(
    XID x_window,
    const SelectionFormatMap& selection)
    : XOSExchangeDataProvider(x_window, selection) {}

OSExchangeDataProviderX11::OSExchangeDataProviderX11() {
  X11EventSource::GetInstance()->AddXEventDispatcher(this);
}

OSExchangeDataProviderX11::~OSExchangeDataProviderX11() {
  if (own_window())
    X11EventSource::GetInstance()->RemoveXEventDispatcher(this);
}

std::unique_ptr<OSExchangeDataProvider> OSExchangeDataProviderX11::Clone()
    const {
  std::unique_ptr<OSExchangeDataProviderX11> ret(
      new OSExchangeDataProviderX11());
  ret->set_format_map(format_map());
  return std::move(ret);
}

void OSExchangeDataProviderX11::SetFileContents(
    const base::FilePath& filename,
    const std::string& file_contents) {
  DCHECK(!filename.empty());
  DCHECK(!base::Contains(format_map(), gfx::GetAtom(kMimeTypeMozillaURL)));

  set_file_contents_name(filename);

  // Direct save handling is a complicated juggling affair between this class,
  // SelectionFormat, and DesktopDragDropClientAuraX11. The general idea behind
  // the protocol is this:
  // - The source window sets its XdndDirectSave0 window property to the
  //   proposed filename.
  // - When a target window receives the drop, it updates the XdndDirectSave0
  //   property on the source window to the filename it would like the contents
  //   to be saved to and then requests the XdndDirectSave0 type from the
  //   source.
  // - The source is supposed to copy the file here and return success (S),
  //   failure (F), or error (E).
  // - In this case, failure means the destination should try to populate the
  //   file itself by copying the data from application/octet-stream. To make
  //   things simpler for Chrome, we always 'fail' and let the destination do
  //   the work.
  std::string failure("F");
  InsertData(gfx::GetAtom("XdndDirectSave0"),
             scoped_refptr<base::RefCountedMemory>(
                 base::RefCountedString::TakeString(&failure)));
  std::string file_contents_copy = file_contents;
  InsertData(gfx::GetAtom("application/octet-stream"),
             scoped_refptr<base::RefCountedMemory>(
                 base::RefCountedString::TakeString(&file_contents_copy)));
}

bool OSExchangeDataProviderX11::DispatchXEvent(XEvent* xev) {
  if (xev->type == SelectionRequest && xev->xany.window == x_window()) {
    selection_owner().OnSelectionRequest(*xev);
    return true;
  }
  return false;
}

}  // namespace ui
