// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RLZ_CHROMEOS_LIB_RLZ_VALUE_STORE_CHROMEOS_H_
#define RLZ_CHROMEOS_LIB_RLZ_VALUE_STORE_CHROMEOS_H_

#include "base/prefs/persistent_pref_store.h"
#include "base/threading/non_thread_safe.h"
#include "rlz/lib/rlz_value_store.h"

namespace base {
class ListValue;
class SequencedTaskRunner;
class Value;
}

template <typename T> struct DefaultSingletonTraits;

namespace rlz_lib {

// An implementation of RlzValueStore for ChromeOS. Unlike Mac and Win
// counterparts, it's non thread-safe and should only be accessed on a single
// Thread instance that has a MessageLoop.
class RlzValueStoreChromeOS : public RlzValueStore {
 public:
  static RlzValueStoreChromeOS* GetInstance();

  // Sets the MessageLoopProxy that underlying PersistentPrefStore will post I/O
  // tasks to. Must be called before the first GetInstance() call.
  static void SetIOTaskRunner(base::SequencedTaskRunner* io_task_runner);

  // Must be invoked during shutdown to commit pending I/O.
  static void Cleanup();

  // Resets the store to its initial state. Should only be used for testing.
  // Same restrictions as for calling GetInstance() for the first time apply,
  // i.e. must call SetIOTaskRunner first.
  static void ResetForTesting();

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
  friend struct DefaultSingletonTraits<RlzValueStoreChromeOS>;

  // Used by JsonPrefStore for write operations.
  static base::SequencedTaskRunner* io_task_runner_;

  static bool created_;

  RlzValueStoreChromeOS();
  virtual ~RlzValueStoreChromeOS();

  // Initializes RLZ store.
  void ReadPrefs();

  // Retrieves list at path |list_name| from JSON store.
  base::ListValue* GetList(std::string list_name);
  // Adds |value| to list at |list_name| path in JSON store.
  bool AddValueToList(std::string list_name, base::Value* value);
  // Removes |value| from list at |list_name| path in JSON store.
  bool RemoveValueFromList(std::string list_name, const base::Value& value);

  // Store with RLZ data.
  scoped_refptr<PersistentPrefStore> rlz_store_;

  DISALLOW_COPY_AND_ASSIGN(RlzValueStoreChromeOS);
};

}  // namespace rlz_lib

#endif  // RLZ_CHROMEOS_LIB_RLZ_VALUE_STORE_CHROMEOS_H_
