// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RLZ_MAC_LIB_RLZ_VALUE_STORE_MAC_H_
#define RLZ_MAC_LIB_RLZ_VALUE_STORE_MAC_H_

#include "base/compiler_specific.h"
#include "base/mac/scoped_nsobject.h"
#include "rlz/lib/rlz_value_store.h"

@class NSDictionary;
@class NSMutableDictionary;

namespace rlz_lib {

// An implementation of RlzValueStore for mac. It stores information in a
// plist file in the user's Application Support folder.
class RlzValueStoreMac : public RlzValueStore {
 public:
  virtual bool HasAccess(AccessType type) OVERRIDE;

  virtual bool WritePingTime(Product product, int64 time) OVERRIDE;
  virtual bool ReadPingTime(Product product, int64* time) OVERRIDE;
  virtual bool ClearPingTime(Product product) OVERRIDE;

  virtual bool WriteAccessPointRlz(AccessPoint access_point,
                                   const char* new_rlz) OVERRIDE;
  virtual bool ReadAccessPointRlz(AccessPoint access_point,
                                  char* rlz,
                                  size_t rlz_size) OVERRIDE;
  virtual bool ClearAccessPointRlz(AccessPoint access_point) OVERRIDE;

  virtual bool AddProductEvent(Product product, const char* event_rlz) OVERRIDE;
  virtual bool ReadProductEvents(Product product,
                                 std::vector<std::string>* events) OVERRIDE;
  virtual bool ClearProductEvent(Product product,
                                 const char* event_rlz) OVERRIDE;
  virtual bool ClearAllProductEvents(Product product) OVERRIDE;

  virtual bool AddStatefulEvent(Product product,
                                const char* event_rlz) OVERRIDE;
  virtual bool IsStatefulEvent(Product product,
                               const char* event_rlz) OVERRIDE;
  virtual bool ClearAllStatefulEvents(Product product) OVERRIDE;

  virtual void CollectGarbage() OVERRIDE;

 private:
  // |dict| is the dictionary that backs all data. plist_path is the name of the
  // plist file, used solely for implementing HasAccess().
  RlzValueStoreMac(NSMutableDictionary* dict, NSString* plist_path);
  virtual ~RlzValueStoreMac();
  friend class ScopedRlzValueStoreLock;

  // Returns the backing dictionary that should be written to disk.
  NSDictionary* dictionary();

  // Returns the dictionary to which all data should be written. Usually, this
  // is just |dictionary()|, but if supplementary branding is used, it's a
  // subdirectory at key "brand_<supplementary branding code>".
  // Note that windows stores data at
  //    rlz/name (e.g. "pingtime")/supplementalbranding/productcode
  // Mac on the other hand does
  //    supplementalbranding/productcode/pingtime.
  NSMutableDictionary* WorkingDict();

  // Returns the subdirectory of |WorkingDict()| used to store data for
  // product p.
  NSMutableDictionary* ProductDict(Product p);

  base::scoped_nsobject<NSMutableDictionary> dict_;
  base::scoped_nsobject<NSString> plist_path_;

  DISALLOW_COPY_AND_ASSIGN(RlzValueStoreMac);
};

}  // namespace rlz_lib

#endif  // RLZ_MAC_LIB_RLZ_VALUE_STORE_MAC_H_
