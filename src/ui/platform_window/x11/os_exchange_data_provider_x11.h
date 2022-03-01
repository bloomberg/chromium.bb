// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_X11_OS_EXCHANGE_DATA_PROVIDER_X11_H_
#define UI_PLATFORM_WINDOW_X11_OS_EXCHANGE_DATA_PROVIDER_X11_H_

#include "ui/base/x/x11_os_exchange_data_provider.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/platform_window/x11/x11_window_export.h"

namespace ui {

// OSExchangeDataProvider implementation for x11 linux.
class X11_WINDOW_EXPORT OSExchangeDataProviderX11
    : public XOSExchangeDataProvider,
      public x11::EventObserver {
 public:
  // |x_window| is the window the cursor is over, |source_window| is the window
  // where the drag started, and |selection| is the set of data being offered.
  OSExchangeDataProviderX11(x11::Window x_window,
                            x11::Window source_window,
                            const SelectionFormatMap& selection);

  // Creates a Provider for sending drag information. This creates its own,
  // hidden X11 window to own send data.
  OSExchangeDataProviderX11();

  ~OSExchangeDataProviderX11() override;

  OSExchangeDataProviderX11(const OSExchangeDataProviderX11&) = delete;
  OSExchangeDataProviderX11& operator=(const OSExchangeDataProviderX11&) =
      delete;

  // OSExchangeDataProvider:
  std::unique_ptr<OSExchangeDataProvider> Clone() const override;

  // x11::EventObserver:
  void OnEvent(const x11::Event& xev) override;

  void SetSource(std::unique_ptr<DataTransferEndpoint> data_source) override;
  DataTransferEndpoint* GetSource() const override;

 private:
  friend class OSExchangeDataProviderX11Test;
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_X11_OS_EXCHANGE_DATA_PROVIDER_X11_H_
