// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DOM_STORAGE_DOM_STORAGE_HOST_H_
#define WEBKIT_DOM_STORAGE_DOM_STORAGE_HOST_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/nullable_string16.h"
#include "base/string16.h"
#include "webkit/dom_storage/dom_storage_types.h"
#include "webkit/storage/webkit_storage_export.h"

class GURL;

namespace dom_storage {

class DomStorageContext;
class DomStorageHost;
class DomStorageNamespace;
class DomStorageArea;

// One instance is allocated in the main process for each client process.
// Used by DomStorageMessageFilter in Chrome and by SimpleDomStorage in DRT.
// This class is single threaded, and performs blocking file reads/writes,
// so it shouldn't be used on chrome's IO thread.
// See class comments for DomStorageContext for a larger overview.
class WEBKIT_STORAGE_EXPORT DomStorageHost {
 public:
  explicit DomStorageHost(DomStorageContext* context);
  ~DomStorageHost();

  bool OpenStorageArea(int connection_id, int namespace_id,
                       const GURL& origin);
  void CloseStorageArea(int connection_id);
  bool ExtractAreaValues(int connection_id, ValuesMap* map);
  unsigned GetAreaLength(int connection_id);
  NullableString16 GetAreaKey(int connection_id, unsigned index);
  NullableString16 GetAreaItem(int connection_id, const base::string16& key);
  bool SetAreaItem(int connection_id, const base::string16& key,
                   const base::string16& value, const GURL& page_url,
                   NullableString16* old_value);
  bool RemoveAreaItem(int connection_id, const base::string16& key,
                  const GURL& page_url,
                  base::string16* old_value);
  bool ClearArea(int connection_id, const GURL& page_url);
  bool HasAreaOpen(int namespace_id, const GURL& origin) const;

 private:
  // Struct to hold references needed for areas that are open
  // within our associated client process.
  struct NamespaceAndArea {
    scoped_refptr<DomStorageNamespace> namespace_;
    scoped_refptr<DomStorageArea> area_;
    NamespaceAndArea();
    ~NamespaceAndArea();
  };
  typedef std::map<int, NamespaceAndArea > AreaMap;

  DomStorageArea* GetOpenArea(int connection_id);

  scoped_refptr<DomStorageContext> context_;
  AreaMap connections_;
};

}  // namespace dom_storage

#endif  // WEBKIT_DOM_STORAGE_DOM_STORAGE_HOST_H_
