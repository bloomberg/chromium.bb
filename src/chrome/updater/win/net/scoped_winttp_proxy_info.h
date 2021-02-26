// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_NET_SCOPED_WINTTP_PROXY_INFO_H_
#define CHROME_UPDATER_WIN_NET_SCOPED_WINTTP_PROXY_INFO_H_

#include <windows.h>
#include <winhttp.h>

#include "base/logging.h"
#include "base/strings/string16.h"

namespace updater {

// Wrapper class for the WINHTTP_PROXY_INFO structure.
// Note that certain Win32 APIs expected the strings to be allocated with
// with GlobalAlloc.
class ScopedWinHttpProxyInfo {
 public:
  ScopedWinHttpProxyInfo() {}

  ScopedWinHttpProxyInfo(const ScopedWinHttpProxyInfo& other) = delete;
  ScopedWinHttpProxyInfo& operator=(const ScopedWinHttpProxyInfo& other) =
      delete;
  ScopedWinHttpProxyInfo(ScopedWinHttpProxyInfo&& other) {
    proxy_info_.lpszProxy = other.proxy_info_.lpszProxy;
    other.proxy_info_.lpszProxy = nullptr;

    proxy_info_.lpszProxyBypass = other.proxy_info_.lpszProxyBypass;
    other.proxy_info_.lpszProxyBypass = nullptr;
  }

  ScopedWinHttpProxyInfo& operator=(ScopedWinHttpProxyInfo&& other) {
    proxy_info_.lpszProxy = other.proxy_info_.lpszProxy;
    other.proxy_info_.lpszProxy = nullptr;

    proxy_info_.lpszProxyBypass = other.proxy_info_.lpszProxyBypass;
    other.proxy_info_.lpszProxyBypass = nullptr;
    return *this;
  }

  ~ScopedWinHttpProxyInfo() {
    if (proxy_info_.lpszProxy)
      ::GlobalFree(proxy_info_.lpszProxy);

    if (proxy_info_.lpszProxyBypass)
      ::GlobalFree(proxy_info_.lpszProxyBypass);
  }

  bool IsValid() const { return proxy_info_.lpszProxy; }

  void set_access_type(DWORD access_type) {
    proxy_info_.dwAccessType = access_type;
  }

  base::char16* proxy() const { return proxy_info_.lpszProxy; }

  void set_proxy(const base::string16& proxy) {
    if (proxy.empty())
      return;

    proxy_info_.lpszProxy = GlobalAlloc(proxy);
  }

  void set_proxy_bypass(const base::string16& proxy_bypass) {
    if (proxy_bypass.empty())
      return;

    proxy_info_.lpszProxyBypass = GlobalAlloc(proxy_bypass);
  }

  // Return the raw pointer since WinHttpSetOption requires a non const pointer.
  const WINHTTP_PROXY_INFO* get() const { return &proxy_info_; }

  WINHTTP_PROXY_INFO* receive() { return &proxy_info_; }

 private:
  base::char16* GlobalAlloc(const base::string16& str) {
    const size_t size_in_bytes = (str.length() + 1) * sizeof(base::char16);
    base::char16* string_mem =
        reinterpret_cast<base::char16*>(::GlobalAlloc(GPTR, size_in_bytes));

    if (!string_mem) {
      PLOG(ERROR) << "GlobalAlloc failed to allocate " << size_in_bytes
                  << " bytes";
      return nullptr;
    }

    memcpy(string_mem, str.data(), size_in_bytes);
    return string_mem;
  }
  WINHTTP_PROXY_INFO proxy_info_ = {};
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_NET_SCOPED_WINTTP_PROXY_INFO_H_
