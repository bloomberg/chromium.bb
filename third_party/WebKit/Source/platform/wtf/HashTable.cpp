/*
    Copyright (C) 2005 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "platform/wtf/HashTable.h"

#if DUMP_HASHTABLE_STATS || DUMP_HASHTABLE_STATS_PER_TABLE

#include "platform/wtf/DataLog.h"
#include "platform/wtf/ThreadingPrimitives.h"

namespace WTF {

static Mutex& hashTableStatsMutex() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, mutex, ());
  return mutex;
}

HashTableStats& HashTableStats::instance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(HashTableStats, stats, ());
  return stats;
}

void HashTableStats::copy(const HashTableStats* other) {
  numAccesses = other->numAccesses;
  numRehashes = other->numRehashes;
  numRemoves = other->numRemoves;
  numReinserts = other->numReinserts;

  maxCollisions = other->maxCollisions;
  numCollisions = other->numCollisions;
  memcpy(collisionGraph, other->collisionGraph, sizeof(collisionGraph));
}

void HashTableStats::recordCollisionAtCount(int count) {
  // The global hash table singleton needs to be atomically updated.
  if (this == &instance()) {
    MutexLocker locker(hashTableStatsMutex());
    RecordCollisionAtCountWithoutLock(count);
  } else {
    RecordCollisionAtCountWithoutLock(count);
  }
}

void HashTableStats::RecordCollisionAtCountWithoutLock(int count) {
  if (count > maxCollisions)
    maxCollisions = count;
  numCollisions++;
  collisionGraph[count]++;
}

void HashTableStats::DumpStats() {
  // Lock the global hash table singleton while dumping.
  if (this == &instance()) {
    MutexLocker locker(hashTableStatsMutex());
    DumpStatsWithoutLock();
  } else {
    DumpStatsWithoutLock();
  }
}

void HashTableStats::DumpStatsWithoutLock() {
  DeprecatedDataLogF("\nWTF::HashTable statistics\n\n");
  DeprecatedDataLogF("%d accesses\n", numAccesses);
  DeprecatedDataLogF("%d total collisions, average %.2f probes per access\n",
                     numCollisions,
                     1.0 * (numAccesses + numCollisions) / numAccesses);
  DeprecatedDataLogF("longest collision chain: %d\n", maxCollisions);
  for (int i = 1; i <= maxCollisions; i++) {
    DeprecatedDataLogF(
        "  %d lookups with exactly %d collisions (%.2f%% , %.2f%% with this "
        "many or more)\n",
        collisionGraph[i], i,
        100.0 * (collisionGraph[i] - collisionGraph[i + 1]) / numAccesses,
        100.0 * collisionGraph[i] / numAccesses);
  }
  DeprecatedDataLogF("%d rehashes\n", numRehashes);
  DeprecatedDataLogF("%d reinserts\n", numReinserts);
}

}  // namespace WTF

#endif
