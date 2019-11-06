// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_ACK_HANDLE_H_
#define COMPONENTS_INVALIDATION_PUBLIC_ACK_HANDLE_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "components/invalidation/public/invalidation_export.h"

namespace base {
class DictionaryValue;
}

namespace syncer {

// Opaque class that represents a local ack handle. We don't reuse the
// invalidation ack handles to avoid unnecessary dependencies.
class INVALIDATION_EXPORT AckHandle {
 public:
  static AckHandle CreateUnique();
  static AckHandle InvalidAckHandle();

  bool Equals(const AckHandle& other) const;

  std::unique_ptr<base::DictionaryValue> ToValue() const;
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

#endif  // COMPONENTS_INVALIDATION_PUBLIC_ACK_HANDLE_H_
