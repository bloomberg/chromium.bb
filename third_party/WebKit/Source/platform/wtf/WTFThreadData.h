/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef WTFThreadData_h
#define WTFThreadData_h

#include <memory>
#include "build/build_config.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/ThreadSpecific.h"
#include "platform/wtf/Threading.h"
#include "platform/wtf/WTFExport.h"
#include "platform/wtf/text/StringHash.h"

namespace WTF {

class AtomicStringTable;
struct ICUConverterWrapper;

class WTF_EXPORT WTFThreadData {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  WTF_MAKE_NONCOPYABLE(WTFThreadData);

 public:
  WTFThreadData();
  ~WTFThreadData();

  AtomicStringTable& GetAtomicStringTable() { return *atomic_string_table_; }

  ICUConverterWrapper& CachedConverterICU() { return *cached_converter_icu_; }

  ThreadIdentifier ThreadId() const { return thread_id_; }

  // Must be called on the main thread before any callers to wtfThreadData().
  static void Initialize();

#if defined(OS_WIN) && defined(COMPILER_MSVC)
  static size_t ThreadStackSize();
#endif

 private:
  std::unique_ptr<AtomicStringTable> atomic_string_table_;
  std::unique_ptr<ICUConverterWrapper> cached_converter_icu_;

  ThreadIdentifier thread_id_;

#if defined(OS_WIN) && defined(COMPILER_MSVC)
  size_t thread_stack_size_ = 0u;
#endif

  static ThreadSpecific<WTFThreadData>* static_data_;
  friend WTFThreadData& WtfThreadData();
};

inline WTFThreadData& WtfThreadData() {
  DCHECK(WTFThreadData::static_data_);
  return **WTFThreadData::static_data_;
}

}  // namespace WTF

using WTF::WTFThreadData;
using WTF::WtfThreadData;

#endif  // WTFThreadData_h
