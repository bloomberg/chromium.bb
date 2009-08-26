// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_ENTRY_H_
#define WEBKIT_APPCACHE_APPCACHE_ENTRY_H_

#include "googleurl/src/gurl.h"

namespace appcache {

// A cached entry is identified by a URL and is classified into one
// (or more) categories.  URL is not stored here as this class is stored
// with the URL as a map key or the user of this class already knows the URL.
class AppCacheEntry {
 public:

  // An entry can be of more than one type so use a bitmask.
  enum Type {
    MASTER = 1 << 0,
    MANIFEST = 1 << 1,
    EXPLICIT = 1 << 2,
    FOREIGN = 1 << 3,
    FALLBACK = 1 << 4,
  };

  explicit AppCacheEntry(int type) : types_(type) {}

  int types() const { return types_; }
  void add_types(int added_types) { types_ |= added_types; }
  bool IsMaster() const { return types_ & MASTER; }
  bool IsManifest() const { return types_ & MANIFEST; }
  bool IsExplicit() const { return types_ & EXPLICIT; }
  bool IsForeign() const { return types_ & FOREIGN; }
  bool IsFallback() const { return types_ & FALLBACK; }

 private:
  int types_;

  // TODO(jennb): response storage key
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_RESOURCE_H_
