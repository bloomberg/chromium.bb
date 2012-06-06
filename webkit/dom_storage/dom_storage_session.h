// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DOM_STORAGE_DOM_STORAGE_SESSION_H_
#define WEBKIT_DOM_STORAGE_DOM_STORAGE_SESSION_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

namespace dom_storage {

class DomStorageContext;

// This refcounted class determines the lifetime of a session
// storage namespace and provides an interface to Clone() an
// existing session storage namespace. It may be used on any thread.
// See class comments for DomStorageContext for a larger overview.
class DomStorageSession
    : public base::RefCountedThreadSafe<DomStorageSession> {
 public:
  explicit DomStorageSession(DomStorageContext* context);
  int64 namespace_id() const { return namespace_id_; }
  DomStorageSession* Clone();

  static DomStorageSession* CloneFrom(DomStorageContext* context,
                                      int64 namepace_id_to_clone);

 private:
  friend class base::RefCountedThreadSafe<DomStorageSession>;

  DomStorageSession(DomStorageContext* context, int64 namespace_id);
  ~DomStorageSession();

  scoped_refptr<DomStorageContext> context_;
  int64 namespace_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DomStorageSession);
};

}  // namespace dom_storage

#endif  // WEBKIT_DOM_STORAGE_DOM_STORAGE_SESSION_H_
