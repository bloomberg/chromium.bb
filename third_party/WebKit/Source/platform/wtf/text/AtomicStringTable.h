// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WTF_AtomicStringTable_h
#define WTF_AtomicStringTable_h

#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/WTFExport.h"
#include "platform/wtf/WTFThreadData.h"
#include "platform/wtf/text/StringHash.h"
#include "platform/wtf/text/StringImpl.h"

namespace WTF {

// The underlying storage that keeps the map of unique AtomicStrings. This is
// not thread safe and each WTFThreadData has one.
class WTF_EXPORT AtomicStringTable final {
  USING_FAST_MALLOC(AtomicStringTable);
  WTF_MAKE_NONCOPYABLE(AtomicStringTable);

 public:
  AtomicStringTable();
  ~AtomicStringTable();

  // Gets the shared table for the current thread.
  static AtomicStringTable& Instance() {
    return WtfThreadData().GetAtomicStringTable();
  }

  // Used by system initialization to preallocate enough storage for all of
  // the static strings.
  void ReserveCapacity(unsigned size);

  // Inserting strings into the table. Note that the return value from adding
  // a UChar string may be an LChar string as the table will attempt to
  // convert the string to save memory if possible.
  StringImpl* Add(StringImpl*);
  RefPtr<StringImpl> Add(const LChar* chars, unsigned length);
  RefPtr<StringImpl> Add(const UChar* chars, unsigned length);

  // Adding UTF8.
  // Returns null if the characters contain invalid utf8 sequences.
  // Pass null for the charactersEnd to automatically detect the length.
  RefPtr<StringImpl> AddUTF8(const char* characters_start,
                             const char* characters_end);

  // This is for ~StringImpl to unregister a string before destruction since
  // the table is holding weak pointers. It should not be used directly.
  void Remove(StringImpl*);

 private:
  template <typename T, typename HashTranslator>
  inline RefPtr<StringImpl> AddToStringTable(const T& value);

  HashSet<StringImpl*> table_;
};

}  // namespace WTF

using WTF::AtomicStringTable;

#endif
