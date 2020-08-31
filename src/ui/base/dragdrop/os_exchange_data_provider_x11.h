// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_X11_H_
#define UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_X11_H_

#include "ui/base/x/x11_os_exchange_data_provider.h"
#include "ui/events/platform/x11/x11_event_source.h"

namespace ui {

// OSExchangeDataProvider implementation for x11 linux.
class UI_BASE_EXPORT OSExchangeDataProviderX11 : public XOSExchangeDataProvider,
                                                 public XEventDispatcher {
 public:
  // |x_window| is the window the cursor is over, and |selection| is the set of
  // data being offered.
  OSExchangeDataProviderX11(XID x_window, const SelectionFormatMap& selection);

  // Creates a Provider for sending drag information. This creates its own,
  // hidden X11 window to own send data.
  OSExchangeDataProviderX11();

  ~OSExchangeDataProviderX11() override;

  OSExchangeDataProviderX11(const OSExchangeDataProviderX11&) = delete;
  OSExchangeDataProviderX11& operator=(const OSExchangeDataProviderX11&) =
      delete;

  // OSExchangeDataProvider:
  std::unique_ptr<OSExchangeDataProvider> Clone() const override;
  void SetFileContents(const base::FilePath& filename,
                       const std::string& file_contents) override;

  // XEventDispatcher:
  bool DispatchXEvent(XEvent* xev) override;

 private:
  friend class OSExchangeDataProviderX11Test;
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_X11_H_
