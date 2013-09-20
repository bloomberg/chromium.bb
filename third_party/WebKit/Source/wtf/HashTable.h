/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Levin <levin@chromium.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef WTF_HashTable_h
#define WTF_HashTable_h

#include "wtf/Alignment.h"
#include "wtf/Assertions.h"
#include "wtf/FastMalloc.h"
#include "wtf/HashTraits.h"
#include <string.h>

#define DUMP_HASHTABLE_STATS 0
#define DUMP_HASHTABLE_STATS_PER_TABLE 0

#if DUMP_HASHTABLE_STATS_PER_TABLE
#include "wtf/DataLog.h"
#endif

namespace WTF {

#if DUMP_HASHTABLE_STATS

    struct HashTableStats {
        // The following variables are all atomically incremented when modified.
        static int numAccesses;
        static int numRehashes;
        static int numRemoves;
        static int numReinserts;

        // The following variables are only modified in the recordCollisionAtCount method within a mutex.
        static int maxCollisions;
        static int numCollisions;
        static int collisionGraph[4096];

        static void recordCollisionAtCount(int count);
        static void dumpStats();
    };

#endif

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    class HashTable;
    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    class HashTableIterator;
    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    class HashTableConstIterator;

    typedef enum { HashItemKnownGood } HashItemKnownGoodTag;

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    class HashTableConstIterator {
    private:
        typedef HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits> HashTableType;
        typedef HashTableIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits> iterator;
        typedef HashTableConstIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits> const_iterator;
        typedef Value ValueType;
        typedef const ValueType& ReferenceType;
        typedef const ValueType* PointerType;

        friend class HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>;
        friend class HashTableIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>;

        void skipEmptyBuckets()
        {
            while (m_position != m_endPosition && HashTableType::isEmptyOrDeletedBucket(*m_position))
                ++m_position;
        }

        HashTableConstIterator(const HashTableType* table, PointerType position, PointerType endPosition)
            : m_position(position), m_endPosition(endPosition)
        {
            skipEmptyBuckets();
        }

        HashTableConstIterator(const HashTableType* table, PointerType position, PointerType endPosition, HashItemKnownGoodTag)
            : m_position(position), m_endPosition(endPosition)
        {
        }

    public:
        HashTableConstIterator()
        {
        }

        PointerType get() const
        {
            return m_position;
        }
        ReferenceType operator*() const { return *get(); }
        PointerType operator->() const { return get(); }

        const_iterator& operator++()
        {
            ASSERT(m_position != m_endPosition);
            ++m_position;
            skipEmptyBuckets();
            return *this;
        }

        // postfix ++ intentionally omitted

        // Comparison.
        bool operator==(const const_iterator& other) const
        {
            return m_position == other.m_position;
        }
        bool operator!=(const const_iterator& other) const
        {
            return m_position != other.m_position;
        }
        bool operator==(const iterator& other) const
        {
            return *this == static_cast<const_iterator>(other);
        }
        bool operator!=(const iterator& other) const
        {
            return *this != static_cast<const_iterator>(other);
        }

    private:
        PointerType m_position;
        PointerType m_endPosition;
    };

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    class HashTableIterator {
    private:
        typedef HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits> HashTableType;
        typedef HashTableIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits> iterator;
        typedef HashTableConstIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits> const_iterator;
        typedef Value ValueType;
        typedef ValueType& ReferenceType;
        typedef ValueType* PointerType;

        friend class HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>;

        HashTableIterator(HashTableType* table, PointerType pos, PointerType end) : m_iterator(table, pos, end) { }
        HashTableIterator(HashTableType* table, PointerType pos, PointerType end, HashItemKnownGoodTag tag) : m_iterator(table, pos, end, tag) { }

    public:
        HashTableIterator() { }

        // default copy, assignment and destructor are OK

        PointerType get() const { return const_cast<PointerType>(m_iterator.get()); }
        ReferenceType operator*() const { return *get(); }
        PointerType operator->() const { return get(); }

        iterator& operator++() { ++m_iterator; return *this; }

        // postfix ++ intentionally omitted

        // Comparison.
        bool operator==(const iterator& other) const { return m_iterator == other.m_iterator; }
        bool operator!=(const iterator& other) const { return m_iterator != other.m_iterator; }
        bool operator==(const const_iterator& other) const { return m_iterator == other; }
        bool operator!=(const const_iterator& other) const { return m_iterator != other; }

        operator const_iterator() const { return m_iterator; }

    private:
        const_iterator m_iterator;
    };

    using std::swap;

    // Work around MSVC's standard library, whose swap for pairs does not swap by component.
    template<typename T> inline void hashTableSwap(T& a, T& b)
    {
        swap(a, b);
    }

    template<typename T, typename U> inline void hashTableSwap(KeyValuePair<T, U>& a, KeyValuePair<T, U>& b)
    {
        swap(a.key, b.key);
        swap(a.value, b.value);
    }

    template<typename T, bool useSwap> struct Mover;
    template<typename T> struct Mover<T, true> { static void move(T& from, T& to) { hashTableSwap(from, to); } };
    template<typename T> struct Mover<T, false> { static void move(T& from, T& to) { to = from; } };

    template<typename HashFunctions> class IdentityHashTranslator {
    public:
        template<typename T> static unsigned hash(const T& key) { return HashFunctions::hash(key); }
        template<typename T, typename U> static bool equal(const T& a, const U& b) { return HashFunctions::equal(a, b); }
        template<typename T, typename U> static void translate(T& location, const U&, const T& value) { location = value; }
    };

    template<typename IteratorType> struct HashTableAddResult {
        HashTableAddResult(IteratorType iter, bool isNewEntry) : iterator(iter), isNewEntry(isNewEntry) { }
        IteratorType iterator;
        bool isNewEntry;
    };

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    class HashTable {
    public:
        typedef HashTableIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits> iterator;
        typedef HashTableConstIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits> const_iterator;
        typedef Traits ValueTraits;
        typedef Key KeyType;
        typedef Value ValueType;
        typedef IdentityHashTranslator<HashFunctions> IdentityTranslatorType;
        typedef HashTableAddResult<iterator> AddResult;

#if DUMP_HASHTABLE_STATS_PER_TABLE
        struct Stats {
            Stats()
                : numAccesses(0)
                , numRehashes(0)
                , numRemoves(0)
                , numReinserts(0)
                , maxCollisions(0)
                , numCollisions(0)
                , collisionGraph()
            {
            }

            int numAccesses;
            int numRehashes;
            int numRemoves;
            int numReinserts;

            int maxCollisions;
            int numCollisions;
            int collisionGraph[4096];

            void recordCollisionAtCount(int count)
            {
                if (count > maxCollisions)
                    maxCollisions = count;
                numCollisions++;
                collisionGraph[count]++;
            }

            void dumpStats()
            {
                dataLogF("\nWTF::HashTable::Stats dump\n\n");
                dataLogF("%d accesses\n", numAccesses);
                dataLogF("%d total collisions, average %.2f probes per access\n", numCollisions, 1.0 * (numAccesses + numCollisions) / numAccesses);
                dataLogF("longest collision chain: %d\n", maxCollisions);
                for (int i = 1; i <= maxCollisions; i++) {
                    dataLogF("  %d lookups with exactly %d collisions (%.2f%% , %.2f%% with this many or more)\n", collisionGraph[i], i, 100.0 * (collisionGraph[i] - collisionGraph[i+1]) / numAccesses, 100.0 * collisionGraph[i] / numAccesses);
                }
                dataLogF("%d rehashes\n", numRehashes);
                dataLogF("%d reinserts\n", numReinserts);
            }
        };
#endif

        HashTable();
        ~HashTable()
        {
            if (LIKELY(!m_table))
                return;
            deallocateTable(m_table, m_tableSize);
            m_table = 0;
        }

        HashTable(const HashTable&);
        void swap(HashTable&);
        HashTable& operator=(const HashTable&);

        // When the hash table is empty, just return the same iterator for end as for begin.
        // This is more efficient because we don't have to skip all the empty and deleted
        // buckets, and iterating an empty table is a common case that's worth optimizing.
        iterator begin() { return isEmpty() ? end() : makeIterator(m_table); }
        iterator end() { return makeKnownGoodIterator(m_table + m_tableSize); }
        const_iterator begin() const { return isEmpty() ? end() : makeConstIterator(m_table); }
        const_iterator end() const { return makeKnownGoodConstIterator(m_table + m_tableSize); }

        unsigned size() const { return m_keyCount; }
        unsigned capacity() const { return m_tableSize; }
        bool isEmpty() const { return !m_keyCount; }

        AddResult add(const ValueType& value) { return add<IdentityTranslatorType>(Extractor::extract(value), value); }

        // A special version of add() that finds the object by hashing and comparing
        // with some other type, to avoid the cost of type conversion if the object is already
        // in the table.
        template<typename HashTranslator, typename T, typename Extra> AddResult add(const T& key, const Extra&);
        template<typename HashTranslator, typename T, typename Extra> AddResult addPassingHashCode(const T& key, const Extra&);

        iterator find(const KeyType& key) { return find<IdentityTranslatorType>(key); }
        const_iterator find(const KeyType& key) const { return find<IdentityTranslatorType>(key); }
        bool contains(const KeyType& key) const { return contains<IdentityTranslatorType>(key); }

        template<typename HashTranslator, typename T> iterator find(const T&);
        template<typename HashTranslator, typename T> const_iterator find(const T&) const;
        template<typename HashTranslator, typename T> bool contains(const T&) const;

        void remove(const KeyType&);
        void remove(iterator);
        void remove(const_iterator);
        void clear();

        static bool isEmptyBucket(const ValueType& value) { return isHashTraitsEmptyValue<KeyTraits>(Extractor::extract(value)); }
        static bool isDeletedBucket(const ValueType& value) { return KeyTraits::isDeletedValue(Extractor::extract(value)); }
        static bool isEmptyOrDeletedBucket(const ValueType& value) { return isEmptyBucket(value) || isDeletedBucket(value); }

        ValueType* lookup(const Key& key) { return lookup<IdentityTranslatorType>(key); }
        template<typename HashTranslator, typename T> ValueType* lookup(const T&);

    private:
        static ValueType* allocateTable(unsigned size);
        static void deallocateTable(ValueType* table, unsigned size);

        typedef std::pair<ValueType*, bool> LookupType;
        typedef std::pair<LookupType, unsigned> FullLookupType;

        LookupType lookupForWriting(const Key& key) { return lookupForWriting<IdentityTranslatorType>(key); };
        template<typename HashTranslator, typename T> FullLookupType fullLookupForWriting(const T&);
        template<typename HashTranslator, typename T> LookupType lookupForWriting(const T&);

        void remove(ValueType*);

        bool shouldExpand() const { return (m_keyCount + m_deletedCount) * m_maxLoad >= m_tableSize; }
        bool mustRehashInPlace() const { return m_keyCount * m_minLoad < m_tableSize * 2; }
        bool shouldShrink() const { return m_keyCount * m_minLoad < m_tableSize && m_tableSize > KeyTraits::minimumTableSize; }
        void expand();
        void shrink() { rehash(m_tableSize / 2); }

        void rehash(unsigned newTableSize);
        void reinsert(ValueType&);

        static void initializeBucket(ValueType& bucket);
        static void deleteBucket(ValueType& bucket) { bucket.~ValueType(); Traits::constructDeletedValue(bucket); }

        FullLookupType makeLookupResult(ValueType* position, bool found, unsigned hash)
            { return FullLookupType(LookupType(position, found), hash); }

        iterator makeIterator(ValueType* pos) { return iterator(this, pos, m_table + m_tableSize); }
        const_iterator makeConstIterator(ValueType* pos) const { return const_iterator(this, pos, m_table + m_tableSize); }
        iterator makeKnownGoodIterator(ValueType* pos) { return iterator(this, pos, m_table + m_tableSize, HashItemKnownGood); }
        const_iterator makeKnownGoodConstIterator(ValueType* pos) const { return const_iterator(this, pos, m_table + m_tableSize, HashItemKnownGood); }

        static const int m_maxLoad = 2;
        static const int m_minLoad = 6;

        ValueType* m_table;
        unsigned m_tableSize;
        unsigned m_tableSizeMask;
        unsigned m_keyCount;
        unsigned m_deletedCount;

#if DUMP_HASHTABLE_STATS_PER_TABLE
    public:
        mutable OwnPtr<Stats> m_stats;
#endif
    };

    // Set all the bits to one after the most significant bit: 00110101010 -> 00111111111.
    template<unsigned size> struct OneifyLowBits;
    template<>
    struct OneifyLowBits<0> {
        static const unsigned value = 0;
    };
    template<unsigned number>
    struct OneifyLowBits {
        static const unsigned value = number | OneifyLowBits<(number >> 1)>::value;
    };
    // Compute the first power of two integer that is an upper bound of the parameter 'number'.
    template<unsigned number>
    struct UpperPowerOfTwoBound {
        static const unsigned value = (OneifyLowBits<number - 1>::value + 1) * 2;
    };

    // Because power of two numbers are the limit of maxLoad, their capacity is twice the
    // UpperPowerOfTwoBound, or 4 times their values.
    template<unsigned size, bool isPowerOfTwo> struct HashTableCapacityForSizeSplitter;
    template<unsigned size>
    struct HashTableCapacityForSizeSplitter<size, true> {
        static const unsigned value = size * 4;
    };
    template<unsigned size>
    struct HashTableCapacityForSizeSplitter<size, false> {
        static const unsigned value = UpperPowerOfTwoBound<size>::value;
    };

    // HashTableCapacityForSize computes the upper power of two capacity to hold the size parameter.
    // This is done at compile time to initialize the HashTraits.
    template<unsigned size>
    struct HashTableCapacityForSize {
        static const unsigned value = HashTableCapacityForSizeSplitter<size, !(size & (size - 1))>::value;
        COMPILE_ASSERT(size > 0, HashTableNonZeroMinimumCapacity);
        COMPILE_ASSERT(!static_cast<int>(value >> 31), HashTableNoCapacityOverflow);
        COMPILE_ASSERT(value > (2 * size), HashTableCapacityHoldsContentSize);
    };

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    inline HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::HashTable()
        : m_table(0)
        , m_tableSize(0)
        , m_tableSizeMask(0)
        , m_keyCount(0)
        , m_deletedCount(0)
#if DUMP_HASHTABLE_STATS_PER_TABLE
        , m_stats(adoptPtr(new Stats))
#endif
    {
    }

    inline unsigned doubleHash(unsigned key)
    {
        key = ~key + (key >> 23);
        key ^= (key << 12);
        key ^= (key >> 7);
        key ^= (key << 2);
        key ^= (key >> 20);
        return key;
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    template<typename HashTranslator, typename T>
    inline Value* HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::lookup(const T& key)
    {
        int k = 0;
        int sizeMask = m_tableSizeMask;
        ValueType* table = m_table;
        unsigned h = HashTranslator::hash(key);
        int i = h & sizeMask;

        if (!table)
            return 0;

#if DUMP_HASHTABLE_STATS
        atomicIncrement(&HashTableStats::numAccesses);
        int probeCount = 0;
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
        ++m_stats->numAccesses;
        int perTableProbeCount = 0;
#endif

        while (1) {
            ValueType* entry = table + i;

            // we count on the compiler to optimize out this branch
            if (HashFunctions::safeToCompareToEmptyOrDeleted) {
                if (HashTranslator::equal(Extractor::extract(*entry), key))
                    return entry;

                if (isEmptyBucket(*entry))
                    return 0;
            } else {
                if (isEmptyBucket(*entry))
                    return 0;

                if (!isDeletedBucket(*entry) && HashTranslator::equal(Extractor::extract(*entry), key))
                    return entry;
            }
#if DUMP_HASHTABLE_STATS
            ++probeCount;
            HashTableStats::recordCollisionAtCount(probeCount);
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
            ++perTableProbeCount;
            m_stats->recordCollisionAtCount(perTableProbeCount);
#endif

            if (!k)
                k = 1 | doubleHash(h);
            i = (i + k) & sizeMask;
        }
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    template<typename HashTranslator, typename T>
    inline typename HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::LookupType HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::lookupForWriting(const T& key)
    {
        ASSERT(m_table);

        int k = 0;
        ValueType* table = m_table;
        int sizeMask = m_tableSizeMask;
        unsigned h = HashTranslator::hash(key);
        int i = h & sizeMask;

#if DUMP_HASHTABLE_STATS
        atomicIncrement(&HashTableStats::numAccesses);
        int probeCount = 0;
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
        ++m_stats->numAccesses;
        int perTableProbeCount = 0;
#endif

        ValueType* deletedEntry = 0;

        while (1) {
            ValueType* entry = table + i;

            // we count on the compiler to optimize out this branch
            if (HashFunctions::safeToCompareToEmptyOrDeleted) {
                if (isEmptyBucket(*entry))
                    return LookupType(deletedEntry ? deletedEntry : entry, false);

                if (HashTranslator::equal(Extractor::extract(*entry), key))
                    return LookupType(entry, true);

                if (isDeletedBucket(*entry))
                    deletedEntry = entry;
            } else {
                if (isEmptyBucket(*entry))
                    return LookupType(deletedEntry ? deletedEntry : entry, false);

                if (isDeletedBucket(*entry))
                    deletedEntry = entry;
                else if (HashTranslator::equal(Extractor::extract(*entry), key))
                    return LookupType(entry, true);
            }
#if DUMP_HASHTABLE_STATS
            ++probeCount;
            HashTableStats::recordCollisionAtCount(probeCount);
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
            ++perTableProbeCount;
            m_stats->recordCollisionAtCount(perTableProbeCount);
#endif

            if (!k)
                k = 1 | doubleHash(h);
            i = (i + k) & sizeMask;
        }
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    template<typename HashTranslator, typename T>
    inline typename HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::FullLookupType HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::fullLookupForWriting(const T& key)
    {
        ASSERT(m_table);

        int k = 0;
        ValueType* table = m_table;
        int sizeMask = m_tableSizeMask;
        unsigned h = HashTranslator::hash(key);
        int i = h & sizeMask;

#if DUMP_HASHTABLE_STATS
        atomicIncrement(&HashTableStats::numAccesses);
        int probeCount = 0;
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
        ++m_stats->numAccesses;
        int perTableProbeCount = 0;
#endif

        ValueType* deletedEntry = 0;

        while (1) {
            ValueType* entry = table + i;

            // we count on the compiler to optimize out this branch
            if (HashFunctions::safeToCompareToEmptyOrDeleted) {
                if (isEmptyBucket(*entry))
                    return makeLookupResult(deletedEntry ? deletedEntry : entry, false, h);

                if (HashTranslator::equal(Extractor::extract(*entry), key))
                    return makeLookupResult(entry, true, h);

                if (isDeletedBucket(*entry))
                    deletedEntry = entry;
            } else {
                if (isEmptyBucket(*entry))
                    return makeLookupResult(deletedEntry ? deletedEntry : entry, false, h);

                if (isDeletedBucket(*entry))
                    deletedEntry = entry;
                else if (HashTranslator::equal(Extractor::extract(*entry), key))
                    return makeLookupResult(entry, true, h);
            }
#if DUMP_HASHTABLE_STATS
            ++probeCount;
            HashTableStats::recordCollisionAtCount(probeCount);
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
            ++perTableProbeCount;
            m_stats->recordCollisionAtCount(perTableProbeCount);
#endif

            if (!k)
                k = 1 | doubleHash(h);
            i = (i + k) & sizeMask;
        }
    }

    template<bool emptyValueIsZero> struct HashTableBucketInitializer;

    template<> struct HashTableBucketInitializer<false> {
        template<typename Traits, typename Value> static void initialize(Value& bucket)
        {
            new (NotNull, &bucket) Value(Traits::emptyValue());
        }
    };

    template<> struct HashTableBucketInitializer<true> {
        template<typename Traits, typename Value> static void initialize(Value& bucket)
        {
            // This initializes the bucket without copying the empty value.
            // That makes it possible to use this with types that don't support copying.
            // The memset to 0 looks like a slow operation but is optimized by the compilers.
            memset(&bucket, 0, sizeof(bucket));
        }
    };

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    inline void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::initializeBucket(ValueType& bucket)
    {
        HashTableBucketInitializer<Traits::emptyValueIsZero>::template initialize<Traits>(bucket);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    template<typename HashTranslator, typename T, typename Extra>
    typename HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::AddResult HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::add(const T& key, const Extra& extra)
    {
        if (!m_table)
            expand();

        ASSERT(m_table);

        int k = 0;
        ValueType* table = m_table;
        int sizeMask = m_tableSizeMask;
        unsigned h = HashTranslator::hash(key);
        int i = h & sizeMask;

#if DUMP_HASHTABLE_STATS
        atomicIncrement(&HashTableStats::numAccesses);
        int probeCount = 0;
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
        ++m_stats->numAccesses;
        int perTableProbeCount = 0;
#endif

        ValueType* deletedEntry = 0;
        ValueType* entry;
        while (1) {
            entry = table + i;

            // we count on the compiler to optimize out this branch
            if (HashFunctions::safeToCompareToEmptyOrDeleted) {
                if (isEmptyBucket(*entry))
                    break;

                if (HashTranslator::equal(Extractor::extract(*entry), key))
                    return AddResult(makeKnownGoodIterator(entry), false);

                if (isDeletedBucket(*entry))
                    deletedEntry = entry;
            } else {
                if (isEmptyBucket(*entry))
                    break;

                if (isDeletedBucket(*entry))
                    deletedEntry = entry;
                else if (HashTranslator::equal(Extractor::extract(*entry), key))
                    return AddResult(makeKnownGoodIterator(entry), false);
            }
#if DUMP_HASHTABLE_STATS
            ++probeCount;
            HashTableStats::recordCollisionAtCount(probeCount);
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
            ++perTableProbeCount;
            m_stats->recordCollisionAtCount(perTableProbeCount);
#endif

            if (!k)
                k = 1 | doubleHash(h);
            i = (i + k) & sizeMask;
        }

        if (deletedEntry) {
            initializeBucket(*deletedEntry);
            entry = deletedEntry;
            --m_deletedCount;
        }

        HashTranslator::translate(*entry, key, extra);

        ++m_keyCount;

        if (shouldExpand()) {
            // FIXME: This makes an extra copy on expand. Probably not that bad since
            // expand is rare, but would be better to have a version of expand that can
            // follow a pivot entry and return the new position.
            KeyType enteredKey = Extractor::extract(*entry);
            expand();
            AddResult result(find(enteredKey), true);
            ASSERT(result.iterator != end());
            return result;
        }

        return AddResult(makeKnownGoodIterator(entry), true);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    template<typename HashTranslator, typename T, typename Extra>
    typename HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::AddResult HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::addPassingHashCode(const T& key, const Extra& extra)
    {
        if (!m_table)
            expand();

        FullLookupType lookupResult = fullLookupForWriting<HashTranslator>(key);

        ValueType* entry = lookupResult.first.first;
        bool found = lookupResult.first.second;
        unsigned h = lookupResult.second;

        if (found)
            return AddResult(makeKnownGoodIterator(entry), false);

        if (isDeletedBucket(*entry)) {
            initializeBucket(*entry);
            --m_deletedCount;
        }

        HashTranslator::translate(*entry, key, extra, h);
        ++m_keyCount;
        if (shouldExpand()) {
            // FIXME: This makes an extra copy on expand. Probably not that bad since
            // expand is rare, but would be better to have a version of expand that can
            // follow a pivot entry and return the new position.
            KeyType enteredKey = Extractor::extract(*entry);
            expand();
            AddResult result(find(enteredKey), true);
            ASSERT(result.iterator != end());
            return result;
        }

        return AddResult(makeKnownGoodIterator(entry), true);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    inline void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::reinsert(ValueType& entry)
    {
        ASSERT(m_table);
        ASSERT(!lookupForWriting(Extractor::extract(entry)).second);
        ASSERT(!isDeletedBucket(*(lookupForWriting(Extractor::extract(entry)).first)));
#if DUMP_HASHTABLE_STATS
        atomicIncrement(&HashTableStats::numReinserts);
#endif
#if DUMP_HASHTABLE_STATS_PER_TABLE
        ++m_stats->numReinserts;
#endif

        Mover<ValueType, Traits::needsDestruction>::move(entry, *lookupForWriting(Extractor::extract(entry)).first);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    template <typename HashTranslator, typename T>
    typename HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::iterator HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::find(const T& key)
    {
        if (!m_table)
            return end();

        ValueType* entry = lookup<HashTranslator>(key);
        if (!entry)
            return end();

        return makeKnownGoodIterator(entry);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    template <typename HashTranslator, typename T>
    typename HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::const_iterator HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::find(const T& key) const
    {
        if (!m_table)
            return end();

        ValueType* entry = const_cast<HashTable*>(this)->lookup<HashTranslator>(key);
        if (!entry)
            return end();

        return makeKnownGoodConstIterator(entry);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    template <typename HashTranslator, typename T>
    bool HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::contains(const T& key) const
    {
        if (!m_table)
            return false;

        return const_cast<HashTable*>(this)->lookup<HashTranslator>(key);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::remove(ValueType* pos)
    {
#if DUMP_HASHTABLE_STATS
        atomicIncrement(&HashTableStats::numRemoves);
#endif
#if DUMP_HASHTABLE_STATS_PER_TABLE
        ++m_stats->numRemoves;
#endif

        deleteBucket(*pos);
        ++m_deletedCount;
        --m_keyCount;

        if (shouldShrink())
            shrink();
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    inline void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::remove(iterator it)
    {
        if (it == end())
            return;

        remove(const_cast<ValueType*>(it.m_iterator.m_position));
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    inline void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::remove(const_iterator it)
    {
        if (it == end())
            return;

        remove(const_cast<ValueType*>(it.m_position));
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    inline void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::remove(const KeyType& key)
    {
        remove(find(key));
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    Value* HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::allocateTable(unsigned size)
    {
        // would use a template member function with explicit specializations here, but
        // gcc doesn't appear to support that
        if (Traits::emptyValueIsZero)
            return static_cast<ValueType*>(fastZeroedMalloc(size * sizeof(ValueType)));
        ValueType* result = static_cast<ValueType*>(fastMalloc(size * sizeof(ValueType)));
        for (unsigned i = 0; i < size; i++)
            initializeBucket(result[i]);
        return result;
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::deallocateTable(ValueType* table, unsigned size)
    {
        if (Traits::needsDestruction) {
            for (unsigned i = 0; i < size; ++i) {
                if (!isDeletedBucket(table[i]))
                    table[i].~ValueType();
            }
        }
        fastFree(table);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::expand()
    {
        unsigned newSize;
        if (!m_tableSize) {
            newSize = KeyTraits::minimumTableSize;
        } else if (mustRehashInPlace()) {
            newSize = m_tableSize;
        } else {
            newSize = m_tableSize * 2;
            RELEASE_ASSERT(newSize > m_tableSize);
        }

        rehash(newSize);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::rehash(unsigned newTableSize)
    {
        unsigned oldTableSize = m_tableSize;
        ValueType* oldTable = m_table;

#if DUMP_HASHTABLE_STATS
        if (oldTableSize != 0)
            atomicIncrement(&HashTableStats::numRehashes);
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
        if (oldTableSize != 0)
            ++m_stats->numRehashes;
#endif

        m_tableSize = newTableSize;
        m_tableSizeMask = newTableSize - 1;
        m_table = allocateTable(newTableSize);

        for (unsigned i = 0; i != oldTableSize; ++i)
            if (!isEmptyOrDeletedBucket(oldTable[i]))
                reinsert(oldTable[i]);

        m_deletedCount = 0;

        deallocateTable(oldTable, oldTableSize);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::clear()
    {
        if (!m_table)
            return;

        deallocateTable(m_table, m_tableSize);
        m_table = 0;
        m_tableSize = 0;
        m_tableSizeMask = 0;
        m_keyCount = 0;
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::HashTable(const HashTable& other)
        : m_table(0)
        , m_tableSize(0)
        , m_tableSizeMask(0)
        , m_keyCount(0)
        , m_deletedCount(0)
#if DUMP_HASHTABLE_STATS_PER_TABLE
        , m_stats(adoptPtr(new Stats(*other.m_stats)))
#endif
    {
        // Copy the hash table the dumb way, by adding each element to the new table.
        // It might be more efficient to copy the table slots, but it's not clear that efficiency is needed.
        const_iterator end = other.end();
        for (const_iterator it = other.begin(); it != end; ++it)
            add(*it);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::swap(HashTable& other)
    {
        ValueType* tmp_table = m_table;
        m_table = other.m_table;
        other.m_table = tmp_table;

        int tmp_tableSize = m_tableSize;
        m_tableSize = other.m_tableSize;
        other.m_tableSize = tmp_tableSize;

        int tmp_tableSizeMask = m_tableSizeMask;
        m_tableSizeMask = other.m_tableSizeMask;
        other.m_tableSizeMask = tmp_tableSizeMask;

        int tmp_keyCount = m_keyCount;
        m_keyCount = other.m_keyCount;
        other.m_keyCount = tmp_keyCount;

        int tmp_deletedCount = m_deletedCount;
        m_deletedCount = other.m_deletedCount;
        other.m_deletedCount = tmp_deletedCount;

#if DUMP_HASHTABLE_STATS_PER_TABLE
        m_stats.swap(other.m_stats);
#endif
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>& HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::operator=(const HashTable& other)
    {
        HashTable tmp(other);
        swap(tmp);
        return *this;
    }

    // iterator adapters

    template<typename HashTableType, typename ValueType> struct HashTableConstIteratorAdapter {
        HashTableConstIteratorAdapter() {}
        HashTableConstIteratorAdapter(const typename HashTableType::const_iterator& impl) : m_impl(impl) {}

        const ValueType* get() const { return (const ValueType*)m_impl.get(); }
        const ValueType& operator*() const { return *get(); }
        const ValueType* operator->() const { return get(); }

        HashTableConstIteratorAdapter& operator++() { ++m_impl; return *this; }
        // postfix ++ intentionally omitted

        typename HashTableType::const_iterator m_impl;
    };

    template<typename HashTableType, typename ValueType> struct HashTableIteratorAdapter {
        HashTableIteratorAdapter() {}
        HashTableIteratorAdapter(const typename HashTableType::iterator& impl) : m_impl(impl) {}

        ValueType* get() const { return (ValueType*)m_impl.get(); }
        ValueType& operator*() const { return *get(); }
        ValueType* operator->() const { return get(); }

        HashTableIteratorAdapter& operator++() { ++m_impl; return *this; }
        // postfix ++ intentionally omitted

        operator HashTableConstIteratorAdapter<HashTableType, ValueType>() {
            typename HashTableType::const_iterator i = m_impl;
            return i;
        }

        typename HashTableType::iterator m_impl;
    };

    template<typename T, typename U>
    inline bool operator==(const HashTableConstIteratorAdapter<T, U>& a, const HashTableConstIteratorAdapter<T, U>& b)
    {
        return a.m_impl == b.m_impl;
    }

    template<typename T, typename U>
    inline bool operator!=(const HashTableConstIteratorAdapter<T, U>& a, const HashTableConstIteratorAdapter<T, U>& b)
    {
        return a.m_impl != b.m_impl;
    }

    template<typename T, typename U>
    inline bool operator==(const HashTableIteratorAdapter<T, U>& a, const HashTableIteratorAdapter<T, U>& b)
    {
        return a.m_impl == b.m_impl;
    }

    template<typename T, typename U>
    inline bool operator!=(const HashTableIteratorAdapter<T, U>& a, const HashTableIteratorAdapter<T, U>& b)
    {
        return a.m_impl != b.m_impl;
    }

    // All 4 combinations of ==, != and Const,non const.
    template<typename T, typename U>
    inline bool operator==(const HashTableConstIteratorAdapter<T, U>& a, const HashTableIteratorAdapter<T, U>& b)
    {
        return a.m_impl == b.m_impl;
    }

    template<typename T, typename U>
    inline bool operator!=(const HashTableConstIteratorAdapter<T, U>& a, const HashTableIteratorAdapter<T, U>& b)
    {
        return a.m_impl != b.m_impl;
    }

    template<typename T, typename U>
    inline bool operator==(const HashTableIteratorAdapter<T, U>& a, const HashTableConstIteratorAdapter<T, U>& b)
    {
        return a.m_impl == b.m_impl;
    }

    template<typename T, typename U>
    inline bool operator!=(const HashTableIteratorAdapter<T, U>& a, const HashTableConstIteratorAdapter<T, U>& b)
    {
        return a.m_impl != b.m_impl;
    }

} // namespace WTF

#include "wtf/HashIterators.h"

#endif // WTF_HashTable_h
