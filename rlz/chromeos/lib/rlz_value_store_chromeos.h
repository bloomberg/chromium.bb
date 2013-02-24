// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RLZ_CHROMEOS_LIB_RLZ_VALUE_STORE_CHROMEOS_H_
#define RLZ_CHROMEOS_LIB_RLZ_VALUE_STORE_CHROMEOS_H_

#include "base/files/file_path.h"
#include "base/threading/non_thread_safe.h"
#include "base/values.h"
#include "rlz/lib/rlz_value_store.h"

namespace base {
class ListValue;
class SequencedTaskRunner;
class Value;
}

namespace rlz_lib {

// An implementation of RlzValueStore for ChromeOS.
class RlzValueStoreChromeOS : public RlzValueStore,
                              public base::NonThreadSafe {
 public:
  // // Sets the task runner that will be used by ImportantFileWriter for write
  // // operations.
  // static void SetFileTaskRunner(base::SequencedTaskRunner* file_task_runner);

  // Creates new instance and synchronously reads data from file.
  RlzValueStoreChromeOS(const base::FilePath& store_path);
  virtual ~RlzValueStoreChromeOS();

  // RlzValueStore overrides:
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
  // Reads RLZ store from file.
  void ReadStore();

  // Writes RLZ store back to file.
  void WriteStore();

  // Adds |value| to list at |list_name| path in JSON store.
  bool AddValueToList(std::string list_name, base::Value* value);
  // Removes |value| from list at |list_name| path in JSON store.
  bool RemoveValueFromList(std::string list_name, const base::Value& value);

  // In-memory store with RLZ data.
  scoped_ptr<base::DictionaryValue> rlz_store_;

  base::FilePath store_path_;

  bool read_only_;

  DISALLOW_COPY_AND_ASSIGN(RlzValueStoreChromeOS);
};

}  // namespace rlz_lib

#endif  // RLZ_CHROMEOS_LIB_RLZ_VALUE_STORE_CHROMEOS_H_
