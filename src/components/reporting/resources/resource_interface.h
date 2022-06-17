// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_RESOURCES_RESOURCE_INTERFACE_H_
#define COMPONENTS_REPORTING_RESOURCES_RESOURCE_INTERFACE_H_

#include <cstdint>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

// Interface to resources management by Storage module.
// Must be implemented by the caller base on the platform limitations.
// All APIs are non-blocking.
class ResourceInterface : public base::RefCountedThreadSafe<ResourceInterface> {
 public:
  // Needs to be called before attempting to allocate specified size.
  // Returns true if requested amount can be allocated.
  // After that the caller can actually allocate it or must call
  // |Discard| if decided not to allocate.
  virtual bool Reserve(uint64_t size) = 0;

  // Reverts reservation.
  // Must be called after the specified amount is released.
  virtual void Discard(uint64_t size) = 0;

  // Returns total amount.
  virtual uint64_t GetTotal() const = 0;

  // Returns current used amount.
  virtual uint64_t GetUsed() const = 0;

  // Test only: Sets non-default usage limit.
  virtual void Test_SetTotal(uint64_t test_total) = 0;

 protected:
  friend class base::RefCountedThreadSafe<ResourceInterface>;

  ResourceInterface() = default;
  virtual ~ResourceInterface() = default;
};

// Moveable RAII class used for scoped Reserve-Discard.
//
// Usage:
//  {
//    ScopedReservation reservation(1024u, options.memory_resource());
//    if (!reservation.reserved()) {
//      // Allocation failed.
//      return;
//    }
//    // Allocation succeeded.
//    ...
//  }   // Automatically discarded.
//
// Can be handed over to another owner.
class ScopedReservation {
 public:
  ScopedReservation(uint64_t size,
                    scoped_refptr<ResourceInterface> resource_interface);
  ScopedReservation(ScopedReservation&& other);
  ScopedReservation(const ScopedReservation& other) = delete;
  ScopedReservation& operator=(const ScopedReservation& other) = delete;
  ~ScopedReservation();

  bool reserved() const;
  bool Reduce(uint64_t new_size);

 private:
  const scoped_refptr<ResourceInterface> resource_interface_;
  absl::optional<uint64_t> size_;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_RESOURCES_RESOURCE_INTERFACE_H_
