// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/PagePool.h"

#include "platform/heap/Heap.h"
#include "platform/heap/PageMemory.h"
#include "wtf/Assertions.h"

namespace blink {

PagePool::PagePool() {
  for (int i = 0; i < BlinkGC::NumberOfArenas; ++i) {
    m_pool[i] = nullptr;
  }
}

PagePool::~PagePool() {
  for (int index = 0; index < BlinkGC::NumberOfArenas; ++index) {
    while (PoolEntry* entry = m_pool[index]) {
      m_pool[index] = entry->next;
      PageMemory* memory = entry->data;
      ASSERT(memory);
      delete memory;
      delete entry;
    }
  }
}

void PagePool::add(int index, PageMemory* memory) {
  // When adding a page to the pool we decommit it to ensure it is unused
  // while in the pool.  This also allows the physical memory, backing the
  // page, to be given back to the OS.
  memory->decommit();
  PoolEntry* entry = new PoolEntry(memory, m_pool[index]);
  m_pool[index] = entry;
}

PageMemory* PagePool::take(int index) {
  while (PoolEntry* entry = m_pool[index]) {
    m_pool[index] = entry->next;
    PageMemory* memory = entry->data;
    ASSERT(memory);
    delete entry;
    if (memory->commit())
      return memory;

    // We got some memory, but failed to commit it, try again.
    delete memory;
  }
  return nullptr;
}

}  // namespace blink
