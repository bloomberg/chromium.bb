// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/encryptedmedia/MediaKeyStatusMap.h"

#include "bindings/core/v8/array_buffer_or_array_buffer_view.h"
#include "core/typed_arrays/DOMArrayBuffer.h"
#include "core/typed_arrays/DOMArrayPiece.h"
#include "platform/SharedBuffer.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebData.h"

#include <algorithm>
#include <limits>

namespace blink {

// Represents the key ID and associated status.
class MediaKeyStatusMap::MapEntry final
    : public GarbageCollectedFinalized<MediaKeyStatusMap::MapEntry> {
 public:
  static MapEntry* Create(WebData key_id, const String& status) {
    return new MapEntry(key_id, status);
  }

  virtual ~MapEntry() {}

  DOMArrayBuffer* KeyId() const { return key_id_.Get(); }

  const String& Status() const { return status_; }

  static bool CompareLessThan(MapEntry* a, MapEntry* b) {
    // Compare the keyIds of 2 different MapEntries. Assume that |a| and |b|
    // are not null, but the keyId() may be. KeyIds are compared byte
    // by byte.
    DCHECK(a);
    DCHECK(b);

    // Handle null cases first (which shouldn't happen).
    //    |aKeyId|    |bKeyId|     result
    //      null        null         == (false)
    //      null      not-null       <  (true)
    //    not-null      null         >  (false)
    if (!a->KeyId() || !b->KeyId())
      return b->KeyId();

    // Compare the bytes.
    int result =
        memcmp(a->KeyId()->Data(), b->KeyId()->Data(),
               std::min(a->KeyId()->ByteLength(), b->KeyId()->ByteLength()));
    if (result != 0)
      return result < 0;

    // KeyIds are equal to the shared length, so the shorter string is <.
    DCHECK_NE(a->KeyId()->ByteLength(), b->KeyId()->ByteLength());
    return a->KeyId()->ByteLength() < b->KeyId()->ByteLength();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { visitor->Trace(key_id_); }

 private:
  MapEntry(WebData key_id, const String& status)
      : key_id_(DOMArrayBuffer::Create(RefPtr<SharedBuffer>(key_id))),
        status_(status) {}

  const Member<DOMArrayBuffer> key_id_;
  const String status_;
};

// Represents an Iterator that loops through the set of MapEntrys.
class MapIterationSource final
    : public PairIterable<ArrayBufferOrArrayBufferView,
                          String>::IterationSource {
 public:
  MapIterationSource(MediaKeyStatusMap* map) : map_(map), current_(0) {}

  bool Next(ScriptState* script_state,
            ArrayBufferOrArrayBufferView& key,
            String& value,
            ExceptionState&) override {
    // This simply advances an index and returns the next value if any,
    // so if the iterated object is mutated values may be skipped.
    if (current_ >= map_->size())
      return false;

    const auto& entry = map_->at(current_++);
    key.SetArrayBuffer(entry.KeyId());
    value = entry.Status();
    return true;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(map_);
    PairIterable<ArrayBufferOrArrayBufferView, String>::IterationSource::Trace(
        visitor);
  }

 private:
  // m_map is stored just for keeping it alive. It needs to be kept
  // alive while JavaScript holds the iterator to it.
  const Member<const MediaKeyStatusMap> map_;
  size_t current_;
};

void MediaKeyStatusMap::Clear() {
  entries_.clear();
}

void MediaKeyStatusMap::AddEntry(WebData key_id, const String& status) {
  // Insert new entry into sorted list.
  MapEntry* entry = MapEntry::Create(key_id, status);
  size_t index = 0;
  while (index < entries_.size() &&
         MapEntry::CompareLessThan(entries_[index], entry))
    ++index;
  entries_.insert(index, entry);
}

const MediaKeyStatusMap::MapEntry& MediaKeyStatusMap::at(size_t index) const {
  DCHECK_LT(index, entries_.size());
  return *entries_.at(index);
}

size_t MediaKeyStatusMap::IndexOf(const DOMArrayPiece& key) const {
  for (size_t index = 0; index < entries_.size(); ++index) {
    const auto& current = entries_.at(index)->KeyId();
    if (key == *current)
      return index;
  }

  // Not found, so return an index outside the valid range. The caller
  // must ensure this value is not exposed outside this class.
  return std::numeric_limits<size_t>::max();
}

bool MediaKeyStatusMap::has(const ArrayBufferOrArrayBufferView& key_id) {
  size_t index = IndexOf(key_id);
  return index < entries_.size();
}

ScriptValue MediaKeyStatusMap::get(ScriptState* script_state,
                                   const ArrayBufferOrArrayBufferView& key_id) {
  size_t index = IndexOf(key_id);
  if (index >= entries_.size())
    return ScriptValue(script_state, v8::Undefined(script_state->GetIsolate()));
  return ScriptValue::From(script_state, at(index).Status());
}

PairIterable<ArrayBufferOrArrayBufferView, String>::IterationSource*
MediaKeyStatusMap::StartIteration(ScriptState*, ExceptionState&) {
  return new MapIterationSource(this);
}

DEFINE_TRACE(MediaKeyStatusMap) {
  visitor->Trace(entries_);
}

}  // namespace blink
