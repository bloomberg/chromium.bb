// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_HOST_H_
#define WEBKIT_APPCACHE_APPCACHE_HOST_H_

#include "base/ref_counted.h"

namespace appcache {

class AppCache;
class AppCacheFrontend;
class AppCacheGroup;

// Server-side representation of an application cache host.
class AppCacheHost {
 public:
  AppCacheHost(int host_id, AppCacheFrontend* frontend);
  ~AppCacheHost();

  int host_id() { return host_id_; }
  AppCacheFrontend* frontend() { return frontend_; }

  AppCache* selected_cache() { return selected_cache_; }
  void set_selected_cache(AppCache* cache);

  bool is_selection_pending() {
    return false;  // TODO(michaeln)
  }

 private:
  // identifies the corresponding appcache host in the child process
  int host_id_;

  // application cache associated with this host, if any
  scoped_refptr<AppCache> selected_cache_;

  // The reference to the appcache group ensures the group exists as long
  // as there is a host using a cache belonging to that group.
  scoped_refptr<AppCacheGroup> group_;

  // frontend to deliver notifications about this host to child process
  AppCacheFrontend* frontend_;
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_HOST_H_
