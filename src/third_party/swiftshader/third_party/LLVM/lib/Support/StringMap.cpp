//===--- StringMap.cpp - String Hash table map implementation -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the StringMap class.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringExtras.h"
#include <cassert>
using namespace llvm;

StringMapImpl::StringMapImpl(unsigned InitSize, unsigned itemSize) {
  ItemSize = itemSize;
  
  // If a size is specified, initialize the table with that many buckets.
  if (InitSize) {
    init(InitSize);
    return;
  }
  
  // Otherwise, initialize it with zero buckets to avoid the allocation.
  TheTable = 0;
  NumBuckets = 0;
  NumItems = 0;
  NumTombstones = 0;
}

void StringMapImpl::init(unsigned InitSize) {
  assert((InitSize & (InitSize-1)) == 0 &&
         "Init Size must be a power of 2 or zero!");
  NumBuckets = InitSize ? InitSize : 16;
  NumItems = 0;
  NumTombstones = 0;
  
  TheTable = (ItemBucket*)calloc(NumBuckets+1, sizeof(ItemBucket));
  
  // Allocate one extra bucket, set it to look filled so the iterators stop at
  // end.
  TheTable[NumBuckets].Item = (StringMapEntryBase*)2;
}


/// LookupBucketFor - Look up the bucket that the specified string should end
/// up in.  If it already exists as a key in the map, the Item pointer for the
/// specified bucket will be non-null.  Otherwise, it will be null.  In either
/// case, the FullHashValue field of the bucket will be set to the hash value
/// of the string.
unsigned StringMapImpl::LookupBucketFor(StringRef Name) {
  unsigned HTSize = NumBuckets;
  if (HTSize == 0) {  // Hash table unallocated so far?
    init(16);
    HTSize = NumBuckets;
  }
  unsigned FullHashValue = HashString(Name);
  unsigned BucketNo = FullHashValue & (HTSize-1);
  
  unsigned ProbeAmt = 1;
  int FirstTombstone = -1;
  while (1) {
    ItemBucket &Bucket = TheTable[BucketNo];
    StringMapEntryBase *BucketItem = Bucket.Item;
    // If we found an empty bucket, this key isn't in the table yet, return it.
    if (BucketItem == 0) {
      // If we found a tombstone, we want to reuse the tombstone instead of an
      // empty bucket.  This reduces probing.
      if (FirstTombstone != -1) {
        TheTable[FirstTombstone].FullHashValue = FullHashValue;
        return FirstTombstone;
      }
      
      Bucket.FullHashValue = FullHashValue;
      return BucketNo;
    }
    
    if (BucketItem == getTombstoneVal()) {
      // Skip over tombstones.  However, remember the first one we see.
      if (FirstTombstone == -1) FirstTombstone = BucketNo;
    } else if (Bucket.FullHashValue == FullHashValue) {
      // If the full hash value matches, check deeply for a match.  The common
      // case here is that we are only looking at the buckets (for item info
      // being non-null and for the full hash value) not at the items.  This
      // is important for cache locality.
      
      // Do the comparison like this because Name isn't necessarily
      // null-terminated!
      char *ItemStr = (char*)BucketItem+ItemSize;
      if (Name == StringRef(ItemStr, BucketItem->getKeyLength())) {
        // We found a match!
        return BucketNo;
      }
    }
    
    // Okay, we didn't find the item.  Probe to the next bucket.
    BucketNo = (BucketNo+ProbeAmt) & (HTSize-1);
    
    // Use quadratic probing, it has fewer clumping artifacts than linear
    // probing and has good cache behavior in the common case.
    ++ProbeAmt;
  }
}


/// FindKey - Look up the bucket that contains the specified key. If it exists
/// in the map, return the bucket number of the key.  Otherwise return -1.
/// This does not modify the map.
int StringMapImpl::FindKey(StringRef Key) const {
  unsigned HTSize = NumBuckets;
  if (HTSize == 0) return -1;  // Really empty table?
  unsigned FullHashValue = HashString(Key);
  unsigned BucketNo = FullHashValue & (HTSize-1);
  
  unsigned ProbeAmt = 1;
  while (1) {
    ItemBucket &Bucket = TheTable[BucketNo];
    StringMapEntryBase *BucketItem = Bucket.Item;
    // If we found an empty bucket, this key isn't in the table yet, return.
    if (BucketItem == 0)
      return -1;
    
    if (BucketItem == getTombstoneVal()) {
      // Ignore tombstones.
    } else if (Bucket.FullHashValue == FullHashValue) {
      // If the full hash value matches, check deeply for a match.  The common
      // case here is that we are only looking at the buckets (for item info
      // being non-null and for the full hash value) not at the items.  This
      // is important for cache locality.
      
      // Do the comparison like this because NameStart isn't necessarily
      // null-terminated!
      char *ItemStr = (char*)BucketItem+ItemSize;
      if (Key == StringRef(ItemStr, BucketItem->getKeyLength())) {
        // We found a match!
        return BucketNo;
      }
    }
    
    // Okay, we didn't find the item.  Probe to the next bucket.
    BucketNo = (BucketNo+ProbeAmt) & (HTSize-1);
    
    // Use quadratic probing, it has fewer clumping artifacts than linear
    // probing and has good cache behavior in the common case.
    ++ProbeAmt;
  }
}

/// RemoveKey - Remove the specified StringMapEntry from the table, but do not
/// delete it.  This aborts if the value isn't in the table.
void StringMapImpl::RemoveKey(StringMapEntryBase *V) {
  const char *VStr = (char*)V + ItemSize;
  StringMapEntryBase *V2 = RemoveKey(StringRef(VStr, V->getKeyLength()));
  (void)V2;
  assert(V == V2 && "Didn't find key?");
}

/// RemoveKey - Remove the StringMapEntry for the specified key from the
/// table, returning it.  If the key is not in the table, this returns null.
StringMapEntryBase *StringMapImpl::RemoveKey(StringRef Key) {
  int Bucket = FindKey(Key);
  if (Bucket == -1) return 0;
  
  StringMapEntryBase *Result = TheTable[Bucket].Item;
  TheTable[Bucket].Item = getTombstoneVal();
  --NumItems;
  ++NumTombstones;
  assert(NumItems + NumTombstones <= NumBuckets);

  return Result;
}



/// RehashTable - Grow the table, redistributing values into the buckets with
/// the appropriate mod-of-hashtable-size.
void StringMapImpl::RehashTable() {
  unsigned NewSize;

  // If the hash table is now more than 3/4 full, or if fewer than 1/8 of
  // the buckets are empty (meaning that many are filled with tombstones),
  // grow/rehash the table.
  if (NumItems*4 > NumBuckets*3) {
    NewSize = NumBuckets*2;
  } else if (NumBuckets-(NumItems+NumTombstones) < NumBuckets/8) {
    NewSize = NumBuckets;
  } else {
    return;
  }

  // Allocate one extra bucket which will always be non-empty.  This allows the
  // iterators to stop at end.
  ItemBucket *NewTableArray =(ItemBucket*)calloc(NewSize+1, sizeof(ItemBucket));
  NewTableArray[NewSize].Item = (StringMapEntryBase*)2;
  
  // Rehash all the items into their new buckets.  Luckily :) we already have
  // the hash values available, so we don't have to rehash any strings.
  for (ItemBucket *IB = TheTable, *E = TheTable+NumBuckets; IB != E; ++IB) {
    if (IB->Item && IB->Item != getTombstoneVal()) {
      // Fast case, bucket available.
      unsigned FullHash = IB->FullHashValue;
      unsigned NewBucket = FullHash & (NewSize-1);
      if (NewTableArray[NewBucket].Item == 0) {
        NewTableArray[FullHash & (NewSize-1)].Item = IB->Item;
        NewTableArray[FullHash & (NewSize-1)].FullHashValue = FullHash;
        continue;
      }
      
      // Otherwise probe for a spot.
      unsigned ProbeSize = 1;
      do {
        NewBucket = (NewBucket + ProbeSize++) & (NewSize-1);
      } while (NewTableArray[NewBucket].Item);
      
      // Finally found a slot.  Fill it in.
      NewTableArray[NewBucket].Item = IB->Item;
      NewTableArray[NewBucket].FullHashValue = FullHash;
    }
  }
  
  free(TheTable);
  
  TheTable = NewTableArray;
  NumBuckets = NewSize;
  NumTombstones = 0;
}
