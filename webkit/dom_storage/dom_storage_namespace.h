// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DOM_STORAGE_DOM_STORAGE_NAMESPACE_H_
#define WEBKIT_DOM_STORAGE_DOM_STORAGE_NAMESPACE_H_

#include <map>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "webkit/storage/webkit_storage_export.h"

class GURL;

namespace dom_storage {

class DomStorageArea;
class DomStorageTaskRunner;
class SessionStorageDatabase;

// Container for the set of per-origin Areas.
// See class comments for DomStorageContext for a larger overview.
class WEBKIT_STORAGE_EXPORT DomStorageNamespace
    : public base::RefCountedThreadSafe<DomStorageNamespace> {
 public:
  // Constructor for a LocalStorage namespace with id of 0
  // and an optional backing directory on disk.
  DomStorageNamespace(const base::FilePath& directory,  // may be empty
                      DomStorageTaskRunner* task_runner);

  // Constructor for a SessionStorage namespace with a non-zero id and an
  // optional backing on disk via |session_storage_database| (may be NULL).
  DomStorageNamespace(int64 namespace_id,
                      const std::string& persistent_namespace_id,
                      SessionStorageDatabase* session_storage_database,
                      DomStorageTaskRunner* task_runner);

  int64 namespace_id() const { return namespace_id_; }
  const std::string& persistent_namespace_id() const {
    return persistent_namespace_id_;
  }

  // Returns the storage area for the given origin,
  // creating instance if needed. Each call to open
  // must be balanced with a call to CloseStorageArea.
  DomStorageArea* OpenStorageArea(const GURL& origin);
  void CloseStorageArea(DomStorageArea* area);

  // Returns the area for |origin| if it's open, otherwise NULL.
  DomStorageArea* GetOpenStorageArea(const GURL& origin);

  // Creates a clone of |this| namespace including
  // shallow copies of all contained areas.
  // Should only be called for session storage namespaces.
  DomStorageNamespace* Clone(int64 clone_namespace_id,
                             const std::string& clone_persistent_namespace_id);

  void DeleteLocalStorageOrigin(const GURL& origin);
  void DeleteSessionStorageOrigin(const GURL& origin);
  void PurgeMemory();
  void Shutdown();

 private:
  friend class base::RefCountedThreadSafe<DomStorageNamespace>;

  // Struct to hold references to our contained areas and
  // to keep track of how many tabs have a given area open.
  struct AreaHolder {
    scoped_refptr<DomStorageArea> area_;
    int open_count_;
    AreaHolder();
    AreaHolder(DomStorageArea* area, int count);
    ~AreaHolder();
  };
  typedef std::map<GURL, AreaHolder> AreaMap;

  ~DomStorageNamespace();

  // Returns a pointer to the area holder in our map or NULL.
  AreaHolder* GetAreaHolder(const GURL& origin);

  int64 namespace_id_;
  std::string persistent_namespace_id_;
  base::FilePath directory_;
  AreaMap areas_;
  scoped_refptr<DomStorageTaskRunner> task_runner_;
  scoped_refptr<SessionStorageDatabase> session_storage_database_;
};

}  // namespace dom_storage


#endif  // WEBKIT_DOM_STORAGE_DOM_STORAGE_NAMESPACE_H_
