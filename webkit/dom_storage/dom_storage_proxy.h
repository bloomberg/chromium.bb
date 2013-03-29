// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DOM_STORAGE_CACHED_AREA_H_
#define WEBKIT_DOM_STORAGE_CACHED_AREA_H_

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/nullable_string16.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "webkit/dom_storage/dom_storage_types.h"

namespace dom_storage {

// Abstract interface for cached area, renderer to browser communications.
class DomStorageProxy : public base::RefCounted<DomStorageProxy> {
 public:
  typedef base::Callback<void(bool)> CompletionCallback;

  virtual void LoadArea(int connection_id, ValuesMap* values,
                        const CompletionCallback& callback) = 0;

  virtual void SetItem(int connection_id, const base::string16& key,
                       const base::string16& value, const GURL& page_url,
                       const CompletionCallback& callback) = 0;

  virtual void RemoveItem(int connection_id, const base::string16& key,
                          const GURL& page_url,
                          const CompletionCallback& callback) = 0;

  virtual void ClearArea(int connection_id,
                         const GURL& page_url,
                         const CompletionCallback& callback) = 0;
 protected:
  friend class base::RefCounted<DomStorageProxy>;
  virtual ~DomStorageProxy() {}
};

}  // namespace dom_storage

#endif  // WEBKIT_DOM_STORAGE_DOM_STORAGE_AREA_H_
