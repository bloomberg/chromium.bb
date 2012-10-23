// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DOM_STORAGE_DOM_STORAGE_SESSION_H_
#define WEBKIT_DOM_STORAGE_DOM_STORAGE_SESSION_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "webkit/storage/webkit_storage_export.h"

namespace dom_storage {

class DomStorageContext;

// This refcounted class determines the lifetime of a session
// storage namespace and provides an interface to Clone() an
// existing session storage namespace. It may be used on any thread.
// See class comments for DomStorageContext for a larger overview.
class WEBKIT_STORAGE_EXPORT DomStorageSession
    : public base::RefCountedThreadSafe<DomStorageSession> {
 public:
  // Constructs a |DomStorageSession| and allocates new IDs for it.
  explicit DomStorageSession(DomStorageContext* context);

  // Constructs a |DomStorageSession| and assigns |persistent_namespace_id|
  // to it. Allocates a new non-persistent ID.
  DomStorageSession(DomStorageContext* context,
                    const std::string& persistent_namespace_id);

  int64 namespace_id() const { return namespace_id_; }
  const std::string& persistent_namespace_id() const {
    return persistent_namespace_id_;
  }
  void SetShouldPersist(bool should_persist);
  bool should_persist() const;
  bool IsFromContext(DomStorageContext* context);
  DomStorageSession* Clone();

  // Constructs a |DomStorageSession| by cloning
  // |namespace_id_to_clone|. Allocates new IDs for it.
  static DomStorageSession* CloneFrom(DomStorageContext* context,
                                      int64 namepace_id_to_clone);

 private:
  friend class base::RefCountedThreadSafe<DomStorageSession>;

  DomStorageSession(DomStorageContext* context,
                    int64 namespace_id,
                    const std::string& persistent_namespace_id);
  ~DomStorageSession();

  scoped_refptr<DomStorageContext> context_;
  int64 namespace_id_;
  std::string persistent_namespace_id_;
  bool should_persist_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DomStorageSession);
};

}  // namespace dom_storage

#endif  // WEBKIT_DOM_STORAGE_DOM_STORAGE_SESSION_H_
