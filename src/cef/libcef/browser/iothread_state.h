// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_IOTHREAD_STATE_H_
#define CEF_LIBCEF_BROWSER_IOTHREAD_STATE_H_
#pragma once

#include "include/cef_request_context.h"
#include "include/cef_request_context_handler.h"
#include "include/cef_scheme.h"

#include "libcef/browser/request_context_handler_map.h"

#include "content/public/browser/browser_thread.h"

class GURL;

// Stores state that will be accessed on the IO thread. Life span is controlled
// by CefBrowserContext. Created on the UI thread but accessed and destroyed on
// the IO thread. See browser_context.h for an object relationship diagram.
class CefIOThreadState : public base::RefCountedThreadSafe<
                             CefIOThreadState,
                             content::BrowserThread::DeleteOnIOThread> {
 public:
  CefIOThreadState();

  // See comments in CefRequestContextHandlerMap.
  void AddHandler(int render_process_id,
                  int render_frame_id,
                  int frame_tree_node_id,
                  CefRefPtr<CefRequestContextHandler> handler);
  void RemoveHandler(int render_process_id,
                     int render_frame_id,
                     int frame_tree_node_id);
  CefRefPtr<CefRequestContextHandler> GetHandler(
      int render_process_id,
      int render_frame_id,
      int frame_tree_node_id,
      bool require_frame_match) const;

  // Manage scheme handler factories associated with this context.
  void RegisterSchemeHandlerFactory(const std::string& scheme_name,
                                    const std::string& domain_name,
                                    CefRefPtr<CefSchemeHandlerFactory> factory);
  void ClearSchemeHandlerFactories();
  CefRefPtr<CefSchemeHandlerFactory> GetSchemeHandlerFactory(const GURL& url);

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::IO>;
  friend class base::DeleteHelper<CefIOThreadState>;

  ~CefIOThreadState();

  void InitOnIOThread();

  // Map IDs to CefRequestContextHandler objects.
  CefRequestContextHandlerMap handler_map_;

  // Map (scheme, domain) to factories.
  typedef std::map<std::pair<std::string, std::string>,
                   CefRefPtr<CefSchemeHandlerFactory>>
      SchemeHandlerFactoryMap;
  SchemeHandlerFactoryMap scheme_handler_factory_map_;

  DISALLOW_COPY_AND_ASSIGN(CefIOThreadState);
};

#endif  // CEF_LIBCEF_BROWSER_IOTHREAD_STATE_H_
