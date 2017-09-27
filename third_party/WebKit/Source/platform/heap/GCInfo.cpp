// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/GCInfo.h"

#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"

namespace blink {

// GCInfo indices start from 1 for heap objects, with 0 being treated
// specially as the index for freelist entries and large heap objects.
size_t GCInfoTable::gc_info_index_ = 0;

size_t GCInfoTable::gc_info_table_size_ = 0;
GCInfo const** g_gc_info_table = nullptr;

void GCInfoTable::EnsureGCInfoIndex(const GCInfo* gc_info,
                                    size_t* gc_info_index_slot) {
  DCHECK(gc_info);
  DCHECK(gc_info_index_slot);
  // Keep a global GCInfoTable lock while allocating a new slot.
  DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, mutex, ());
  MutexLocker locker(mutex);

  // If more than one thread ends up allocating a slot for
  // the same GCInfo, have later threads reuse the slot
  // allocated by the first.
  if (*gc_info_index_slot)
    return;

  int index = ++gc_info_index_;
  size_t gc_info_index = static_cast<size_t>(index);
  CHECK(gc_info_index < GCInfoTable::kMaxIndex);
  if (gc_info_index >= gc_info_table_size_)
    Resize();

  g_gc_info_table[gc_info_index] = gc_info;
  ReleaseStore(reinterpret_cast<int*>(gc_info_index_slot), index);
}

void GCInfoTable::Resize() {
  static const int kGcInfoZapValue = 0x33;
  // (Light) experimentation suggests that Blink doesn't need
  // more than this while handling content on popular web properties.
  const size_t kInitialSize = 512;

  size_t new_size =
      gc_info_table_size_ ? 2 * gc_info_table_size_ : kInitialSize;
  DCHECK(new_size < GCInfoTable::kMaxIndex);
  g_gc_info_table =
      reinterpret_cast<GCInfo const**>(WTF::Partitions::FastRealloc(
          g_gc_info_table, new_size * sizeof(GCInfo), "GCInfo"));
  DCHECK(g_gc_info_table);
  memset(reinterpret_cast<uint8_t*>(g_gc_info_table) +
             gc_info_table_size_ * sizeof(GCInfo),
         kGcInfoZapValue, (new_size - gc_info_table_size_) * sizeof(GCInfo));
  gc_info_table_size_ = new_size;
}

void GCInfoTable::Init() {
  CHECK(!g_gc_info_table);
  Resize();
}

#if DCHECK_IS_ON()
void AssertObjectHasGCInfo(const void* payload, size_t gc_info_index) {
  HeapObjectHeader::CheckFromPayload(payload);
#if !defined(COMPONENT_BUILD)
  // On component builds we cannot compare the gcInfos as they are statically
  // defined in each of the components and hence will not match.
  DCHECK_EQ(HeapObjectHeader::FromPayload(payload)->GcInfoIndex(),
            gc_info_index);
#endif
}
#endif

}  // namespace blink
