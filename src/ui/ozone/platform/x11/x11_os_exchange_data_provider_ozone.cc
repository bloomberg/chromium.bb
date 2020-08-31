// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_os_exchange_data_provider_ozone.h"

#include <utility>

#include "base/logging.h"
#include "ui/base/x/selection_utils.h"

namespace ui {

X11OSExchangeDataProviderOzone::X11OSExchangeDataProviderOzone(
    XID x_window,
    const SelectionFormatMap& selection)
    : XOSExchangeDataProvider(x_window, selection) {}

X11OSExchangeDataProviderOzone::X11OSExchangeDataProviderOzone() {
  DCHECK(own_window());
  X11EventSource::GetInstance()->AddXEventDispatcher(this);
}

X11OSExchangeDataProviderOzone::~X11OSExchangeDataProviderOzone() {
  if (own_window())
    X11EventSource::GetInstance()->RemoveXEventDispatcher(this);
}

std::unique_ptr<OSExchangeDataProvider> X11OSExchangeDataProviderOzone::Clone()
    const {
  std::unique_ptr<X11OSExchangeDataProviderOzone> ret(
      new X11OSExchangeDataProviderOzone());
  ret->set_format_map(format_map());
  return std::move(ret);
}

bool X11OSExchangeDataProviderOzone::DispatchXEvent(XEvent* xev) {
  if (xev->xany.window != x_window())
    return false;

  switch (xev->type) {
    case SelectionRequest:
      selection_owner().OnSelectionRequest(*xev);
      return true;
    default:
      NOTIMPLEMENTED();
  }
  return false;
}

}  // namespace ui
