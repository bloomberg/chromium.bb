// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_CORE_ACCOUNT_ID_H_
#define GOOGLE_APIS_GAIA_CORE_ACCOUNT_ID_H_

#include <ostream>
#include <string>

// Represent the id of an account for interaction with GAIA. It is
// currently implicitly convertible to and from std::string to allow
// progressive migration of the code (see https://crbug.com/959157
// for design and tracking).
struct CoreAccountId {
  CoreAccountId();
  ~CoreAccountId();

  // Those implicit constructor and conversion operator allow to
  // progressively migrate the code to use this struct. Removing
  // them is tracked by https://crbug.com/959161
  CoreAccountId(const char* id);
  CoreAccountId(std::string&& id);
  CoreAccountId(const std::string& id);
  operator std::string() const;

  // Checks if the account is valid or not.
  // TODO(triploblastic): Possibly rename of remove this after
  // refactoring.
  bool empty() const;

  std::string id;
};

bool operator<(const CoreAccountId& lhs, const CoreAccountId& rhs);

bool operator==(const CoreAccountId& lhs, const CoreAccountId& rhs);

bool operator!=(const CoreAccountId& lhs, const CoreAccountId& rhs);

std::ostream& operator<<(std::ostream& out, const CoreAccountId& a);

#endif  // GOOGLE_APIS_GAIA_CORE_ACCOUNT_ID_H_
