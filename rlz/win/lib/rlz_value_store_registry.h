// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RLZ_WIN_LIB_RLZ_VALUE_STORE_REGISTRY_H_
#define RLZ_WIN_LIB_RLZ_VALUE_STORE_REGISTRY_H_

#include "base/compiler_specific.h"
#include "rlz/lib/rlz_value_store.h"

namespace rlz_lib {

// Implements RlzValueStore by storing values in the windows registry.
class RlzValueStoreRegistry : public RlzValueStore {
 public:
  static std::wstring GetWideLibKeyName();

  virtual bool HasAccess(AccessType type) override;

  virtual bool WritePingTime(Product product, int64 time) override;
  virtual bool ReadPingTime(Product product, int64* time) override;
  virtual bool ClearPingTime(Product product) override;

  virtual bool WriteAccessPointRlz(AccessPoint access_point,
                                   const char* new_rlz) override;
  virtual bool ReadAccessPointRlz(AccessPoint access_point,
                                  char* rlz,
                                  size_t rlz_size) override;
  virtual bool ClearAccessPointRlz(AccessPoint access_point) override;

  virtual bool AddProductEvent(Product product, const char* event_rlz) override;
  virtual bool ReadProductEvents(Product product,
                                 std::vector<std::string>* events) override;
  virtual bool ClearProductEvent(Product product,
                                 const char* event_rlz) override;
  virtual bool ClearAllProductEvents(Product product) override;

  virtual bool AddStatefulEvent(Product product,
                                const char* event_rlz) override;
  virtual bool IsStatefulEvent(Product product,
                               const char* event_rlz) override;
  virtual bool ClearAllStatefulEvents(Product product) override;

  virtual void CollectGarbage() override;

 private:
  RlzValueStoreRegistry() {}
  DISALLOW_COPY_AND_ASSIGN(RlzValueStoreRegistry);
  friend class ScopedRlzValueStoreLock;
};

}  // namespace rlz_lib

#endif  // RLZ_WIN_LIB_RLZ_VALUE_STORE_REGISTRY_H_

