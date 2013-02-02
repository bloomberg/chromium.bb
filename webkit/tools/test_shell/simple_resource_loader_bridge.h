// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_SIMPLE_RESOURCE_LOADER_BRIDGE_H__
#define WEBKIT_TOOLS_TEST_SHELL_SIMPLE_RESOURCE_LOADER_BRIDGE_H__

#include <string>
#include "base/message_loop_proxy.h"
#include "net/http/http_cache.h"
#include "webkit/glue/resource_loader_bridge.h"

class GURL;

namespace base {
class FilePath;
}

class SimpleResourceLoaderBridge {
 public:
  // Call this function to initialize the simple resource loader bridge.
  // It is safe to call this function multiple times.
  //
  // NOTE: If this function is not called, then a default request context will
  // be initialized lazily.
  //
  static void Init(const base::FilePath& cache_path,
                   net::HttpCache::Mode cache_mode,
                   bool no_proxy);

  // Call this function to shutdown the simple resource loader bridge.
  static void Shutdown();

  // May only be called after Init.
  static void SetCookie(const GURL& url,
                        const GURL& first_party_for_cookies,
                        const std::string& cookie);
  static std::string GetCookies(const GURL& url,
                                const GURL& first_party_for_cookies);
  static bool EnsureIOThread();
  static void SetAcceptAllCookies(bool accept_all_cookies);

  // These methods should only be called after Init(), and before
  // Shutdown(). The MessageLoops get replaced upon each call to
  // Init(), and destroyed upon a call to ShutDown().
  static scoped_refptr<base::MessageLoopProxy> GetCacheThread();
  static scoped_refptr<base::MessageLoopProxy> GetIoThread();

  // Call this function to set up a redirection using the file-over-http
  // feature. If redirections are set up, request using the file scheme which
  // match |file_path_template| will be rewritten to the |http_prefix| plus
  // the path that follows after the |file_path_template| in the request.
  static void AllowFileOverHTTP(const std::string& file_path_template,
                                const GURL& http_prefix);

  // Creates a ResourceLoaderBridge instance.
  static webkit_glue::ResourceLoaderBridge* Create(
    const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info);
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_SIMPLE_RESOURCE_LOADER_BRIDGE_H__
