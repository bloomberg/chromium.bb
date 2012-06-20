// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DOM_STORAGE_SESSION_STORAGE_DATABASE_ADAPTER_H_
#define WEBKIT_DOM_STORAGE_SESSION_STORAGE_DATABASE_ADAPTER_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "googleurl/src/gurl.h"
#include "webkit/dom_storage/dom_storage_database_adapter.h"

namespace dom_storage {

class SessionStorageDatabase;

class SessionStorageDatabaseAdapter : public DomStorageDatabaseAdapter {
 public:
  SessionStorageDatabaseAdapter(SessionStorageDatabase* db,
                                const std::string& permanent_namespace_id,
                                const GURL& origin);
  virtual ~SessionStorageDatabaseAdapter();
  virtual void ReadAllValues(ValuesMap* result) OVERRIDE;
  virtual bool CommitChanges(bool clear_all_first,
                             const ValuesMap& changes) OVERRIDE;
 private:
  scoped_refptr<SessionStorageDatabase> db_;
  std::string permanent_namespace_id_;
  GURL origin_;

  DISALLOW_COPY_AND_ASSIGN(SessionStorageDatabaseAdapter);
};

}  // namespace dom_storage

#endif  // WEBKIT_DOM_STORAGE_SESSION_STORAGE_DATABASE_ADAPTER_H_
