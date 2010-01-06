// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DATABASE_QUOTA_TABLE_H_
#define WEBKIT_DATABASE_QUOTA_TABLE_H_

#include <vector>

#include "base/string16.h"

namespace sql {
class Connection;
}

namespace webkit_database {

class QuotaTable {
 public:
  explicit QuotaTable(sql::Connection* db) : db_(db) { }

  bool Init();
  int64 GetOriginQuota(const string16& origin_identifier);
  bool SetOriginQuota(const string16& origin_identifier, int64 quota);
  bool ClearOriginQuota(const string16& origin_identifier);

 private:
  sql::Connection* db_;
};

}  // namespace webkit_database

#endif  // WEBKIT_DATABASE_QUOTA_TABLE_H_
