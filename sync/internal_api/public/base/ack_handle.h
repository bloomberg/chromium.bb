// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_BASE_ACK_HANDLE_H
#define SYNC_INTERNAL_API_PUBLIC_BASE_ACK_HANDLE_H

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "sync/base/sync_export.h"

namespace base {
class DictionaryValue;
}

namespace syncer {

// Opaque class that represents a local ack handle. We don't reuse the
// invalidation ack handles to avoid unnecessary dependencies.
class SYNC_EXPORT AckHandle {
 public:
  static AckHandle CreateUnique();
  static AckHandle InvalidAckHandle();

  bool Equals(const AckHandle& other) const;

  scoped_ptr<base::DictionaryValue> ToValue() const;
  bool ResetFromValue(const base::DictionaryValue& value);

  bool IsValid() const;

  ~AckHandle();

 private:
  // Explicitly copyable and assignable for STL containers.
  AckHandle(const std::string& state, base::Time timestamp);

  std::string state_;
  base::Time timestamp_;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_BASE_ACK_HANDLE_H
