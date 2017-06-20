// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MemoryCoordinator_h
#define MemoryCoordinator_h

#include "platform/PlatformExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/WebMemoryPressureLevel.h"
#include "public/platform/WebMemoryState.h"
#include "public/platform/WebThread.h"

namespace blink {

class PLATFORM_EXPORT MemoryCoordinatorClient : public GarbageCollectedMixin {
 public:
  virtual ~MemoryCoordinatorClient() {}

  // TODO(bashi): Deprecating. Remove this when MemoryPressureListener is
  // gone.
  virtual void OnMemoryPressure(WebMemoryPressureLevel) {}

  virtual void OnMemoryStateChange(MemoryState) {}

  virtual void OnPurgeMemory() {}
};

// MemoryCoordinator listens to some events which could be opportunities
// for reducing memory consumption and notifies its clients.
class PLATFORM_EXPORT MemoryCoordinator final
    : public GarbageCollectedFinalized<MemoryCoordinator> {
  WTF_MAKE_NONCOPYABLE(MemoryCoordinator);

 public:
  static MemoryCoordinator& Instance();

  // Whether the device Blink runs on is a low-end device.
  // Can be overridden in layout tests via internals.
  static bool IsLowEndDevice();

  // Returns the amount of physical memory in megabytes on the device.
  static int64_t GetPhysicalMemoryMB();

  // Override the value of the physical memory for testing.
  static void SetPhysicalMemoryMBForTesting(int64_t);

  // Returns true when available memory is low.
  // This is not cheap and should not be called repeatedly.
  // TODO(keishi): Remove when MemoryState is ready.
  static bool IsCurrentlyLowMemory();

  // Caches whether this device is a low-end device and the device physical
  // memory in static members. instance() is not used as it's a heap allocated
  // object - meaning it's not thread-safe as well as might break tests counting
  // the heap size.
  static void Initialize();

  static void RegisterThread(WebThread*);
  static void UnregisterThread(WebThread*);

  void RegisterClient(MemoryCoordinatorClient*);
  void UnregisterClient(MemoryCoordinatorClient*);

  // TODO(bashi): Deprecating. Remove this when MemoryPressureListener is
  // gone.
  void OnMemoryPressure(WebMemoryPressureLevel);

  void OnMemoryStateChange(MemoryState);

  void OnPurgeMemory();

  DECLARE_TRACE();

 private:
  friend class Internals;

  static void SetIsLowEndDeviceForTesting(bool);

  MemoryCoordinator();

  void ClearMemory();
  static void ClearThreadSpecificMemory();

  static bool is_low_end_device_;
  static int64_t physical_memory_mb_;

  HeapHashSet<WeakMember<MemoryCoordinatorClient>> clients_;
  HashSet<WebThread*> web_threads_;
};

}  // namespace blink

#endif  // MemoryCoordinator_h
