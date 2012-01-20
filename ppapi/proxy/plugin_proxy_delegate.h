// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_PROXY_DELEGATE_H_
#define PPAPI_PROXY_PLUGIN_PROXY_DELEGATE_H_

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT PluginProxyDelegate {
 public:
  virtual ~PluginProxyDelegate() {}

  // Returns the WebKit forwarding object used to make calls into WebKit.
  // Necessary only on the plugin side.
  virtual WebKitForwarding* GetWebKitForwarding() = 0;

  // Posts the given task to the WebKit thread associated with this plugin
  // process. The WebKit thread should be lazily created if it does not
  // exist yet.
  virtual void PostToWebKitThread(const tracked_objects::Location& from_here,
                                  const base::Closure& task) = 0;

  // Sends the given message to the browser. Identical semantics to
  // IPC::Message::Sender interface.
  virtual bool SendToBrowser(IPC::Message* msg) = 0;

  // Performs Windows-specific font caching in the browser for the given
  // LOGFONTW. Does nothing on non-Windows platforms.
  virtual void PreCacheFont(const void* logfontw) = 0;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PLUGIN_PROXY_DELEGATE_H_
