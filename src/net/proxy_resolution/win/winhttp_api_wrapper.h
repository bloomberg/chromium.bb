// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_WIN_WINHTTP_API_WRAPPER_H_
#define NET_PROXY_RESOLUTION_WIN_WINHTTP_API_WRAPPER_H_

#include <windows.h>
#include <winhttp.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "net/base/net_export.h"

namespace net {

// This is a utility class that encapsulates the memory management necessary for
// WINHTTP_CURRENT_USER_IE_PROXY_CONFIG in RAII style.
class ScopedIEConfig final {
 public:
  ScopedIEConfig();

  ScopedIEConfig(const ScopedIEConfig&) = delete;
  ScopedIEConfig& operator=(const ScopedIEConfig&) = delete;

  ~ScopedIEConfig();

  WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* config() { return &ie_config; }

 private:
  WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ie_config = {0};
};

// This provides a layer of abstraction between calling code and WinHTTP APIs,
// allowing them to be mocked out for testing. This object is not thread safe
// and it's expected that the caller will handle using it on the same thread or
// sequence. In general, documentation for these APIs can be found here:
// https://docs.microsoft.com/en-us/windows/win32/api/winhttp/
class NET_EXPORT WinHttpAPIWrapper {
 public:
  WinHttpAPIWrapper();

  WinHttpAPIWrapper(const WinHttpAPIWrapper&) = delete;
  WinHttpAPIWrapper& operator=(const WinHttpAPIWrapper&) = delete;

  virtual ~WinHttpAPIWrapper();

  // Creates our WinHttp session handle |session_handle_|. The lifetime of that
  // session handle is determined by the lifetime of this object. It'll get
  // closed when this object destructs.
  virtual bool CallWinHttpOpen() WARN_UNUSED_RESULT;

  // Controls the timeout for WinHttpGetProxyForUrlEx().
  virtual bool CallWinHttpSetTimeouts(int resolve_timeout,
                                      int connect_timeout,
                                      int send_timeout,
                                      int receive_timeout) WARN_UNUSED_RESULT;

  // Sets the callback WinHttp will call into with the result of any
  // asynchronous call.
  virtual bool CallWinHttpSetStatusCallback(
      WINHTTP_STATUS_CALLBACK internet_callback) WARN_UNUSED_RESULT;

  // Fetches the proxy configs for the current active network connection and
  // current Windows user. The |ie_proxy_config| says whether or not
  // AutoProxy (WPAD) is enabled and if there's a PAC URL configured for this
  // connection/user.
  virtual bool CallWinHttpGetIEProxyConfigForCurrentUser(
      WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* ie_proxy_config) WARN_UNUSED_RESULT;

  // Creates a handle |resolver_handle| that should be used for the call to
  // WinHttpGetProxyForUrlEx().
  virtual bool CallWinHttpCreateProxyResolver(HINTERNET* out_resolver_handle)
      WARN_UNUSED_RESULT;

  // Using the specific |resolver_handle| handle from
  // CallWinHttpCreateProxyResolver(), resolve a proxy for a specific |url| with
  // the aid of some |autoproxy_options|. When WinHttpGetProxyForUrlEx()
  // finishes its work or hits an error, it'll call into the callback set by
  // CallWinHttpSetStatusCallback() above exactly once and supply the provided
  // |context|.
  //
  // WinHttpGetProxyForUrlEx() will go async to do all necessary logic. As long
  // as it receives good inputs (valid handle, valid combination of flags,
  // non-null PAC URL if needed), this API will almost always return
  // ERROR_IO_PENDING. It'll only fail for reasons like running out of memory.
  // When it returns ERROR_IO_PENDING, CallWinHttpGetProxyForUrlEx() will return
  // true.
  //
  // WinHttpGetProxyForUrlEx() will do proxy fallback internally and return to
  // a proxy result. It will first check WPAD (if enabled). If that fails, it'll
  // attempt to download and run any provided PAC script. If the PAC script was
  // not provided or if it fails, it'll use the right per-interface static
  // proxy. If all else fails or isn't configured, it'll simply return DIRECT.
  // WinHttpGetProxyForUrlEx() supports commonly used enterprise proxy features
  // such as DirectAccess/NRPT.
  virtual bool CallWinHttpGetProxyForUrlEx(
      HINTERNET resolver_handle,
      const std::string& url,
      WINHTTP_AUTOPROXY_OPTIONS* autoproxy_options,
      DWORD_PTR context) WARN_UNUSED_RESULT;

  // As long as CallWinHttpGetProxyForUrlEx() doesn't hit any errors, there will
  // be a proxy result to examine. This function retrieves that proxy resolution
  // result |proxy_result| using the resolver's handle |resolver_handle|. The
  // result must be freed with CallWinHttpFreeProxyResult().
  virtual bool CallWinHttpGetProxyResult(HINTERNET resolver_handle,
                                         WINHTTP_PROXY_RESULT* proxy_result)
      WARN_UNUSED_RESULT;

  // Frees the |proxy_result| retrieved by CallWinHttpGetProxyResult().
  virtual void CallWinHttpFreeProxyResult(WINHTTP_PROXY_RESULT* proxy_result);

  // Every opened HINTERNET handle must be closed. This closes handle
  // |internet_handle|. After being closed, WinHttp calls cannot be made using
  // that handle.
  virtual void CallWinHttpCloseHandle(HINTERNET internet_handle);

 private:
  // Closes |session_handle_|.
  void CloseSessionHandle();

  HINTERNET session_handle_ = nullptr;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_WIN_WINHTTP_API_WRAPPER_H_
