// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_PROXY_DELEGATE_H_
#define PPAPI_PROXY_PLUGIN_PROXY_DELEGATE_H_

#include <string>

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT PluginProxyDelegate {
 public:
  virtual ~PluginProxyDelegate() {}

  // Sends the given message to the browser. Identical semantics to IPC::Sender
  // interface.
  virtual bool SendToBrowser(IPC::Message* msg) = 0;

  // Returns the language code of the current UI language.
  virtual std::string GetUILanguage() = 0;

  // Performs Windows-specific font caching in the browser for the given
  // LOGFONTW. Does nothing on non-Windows platforms.
  virtual void PreCacheFont(const void* logfontw) = 0;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PLUGIN_PROXY_DELEGATE_H_
