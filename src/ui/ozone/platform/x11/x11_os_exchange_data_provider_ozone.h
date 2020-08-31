// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_OS_EXCHANGE_DATA_PROVIDER_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_OS_EXCHANGE_DATA_PROVIDER_OZONE_H_

#include "ui/base/x/x11_os_exchange_data_provider.h"
#include "ui/events/platform/x11/x11_event_source.h"

namespace ui {

// OSExchangeDataProvider implementation for Ozone/X11.
class X11OSExchangeDataProviderOzone : public XOSExchangeDataProvider,
                                       public XEventDispatcher {
 public:
  X11OSExchangeDataProviderOzone(XID x_window,
                                 const SelectionFormatMap& selection);
  X11OSExchangeDataProviderOzone();
  ~X11OSExchangeDataProviderOzone() override;
  X11OSExchangeDataProviderOzone(const X11OSExchangeDataProviderOzone&) =
      delete;
  X11OSExchangeDataProviderOzone& operator=(
      const X11OSExchangeDataProviderOzone&) = delete;

  // OSExchangeDataProvider:
  std::unique_ptr<OSExchangeDataProvider> Clone() const override;

  // XEventDispatcher:
  bool DispatchXEvent(XEvent* xev) override;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_OS_EXCHANGE_DATA_PROVIDER_OZONE_H_
