/*
 * Copyright (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2007, 2008, 2011, 2012 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "core/dom/SpaceSplitString.h"

#include "core/html/parser/HTMLParserIdioms.h"
#include "wtf/ASCIICType.h"
#include "wtf/HashMap.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

template <typename CharacterType>
static inline bool HasNonASCIIOrUpper(const CharacterType* characters,
                                      unsigned length) {
  bool has_upper = false;
  CharacterType ored = 0;
  for (unsigned i = 0; i < length; i++) {
    CharacterType c = characters[i];
    has_upper |= IsASCIIUpper(c);
    ored |= c;
  }
  return has_upper || (ored & ~0x7F);
}

static inline bool HasNonASCIIOrUpper(const String& string) {
  unsigned length = string.length();

  if (string.Is8Bit())
    return HasNonASCIIOrUpper(string.Characters8(), length);
  return HasNonASCIIOrUpper(string.Characters16(), length);
}

template <typename CharacterType>
inline void SpaceSplitString::Data::CreateVector(
    const CharacterType* characters,
    unsigned length) {
  unsigned start = 0;
  while (true) {
    while (start < length && IsHTMLSpace<CharacterType>(characters[start]))
      ++start;
    if (start >= length)
      break;
    unsigned end = start + 1;
    while (end < length && IsNotHTMLSpace<CharacterType>(characters[end]))
      ++end;

    vector_.push_back(AtomicString(characters + start, end - start));

    start = end + 1;
  }
}

void SpaceSplitString::Data::CreateVector(const String& string) {
  unsigned length = string.length();

  if (string.Is8Bit()) {
    CreateVector(string.Characters8(), length);
    return;
  }

  CreateVector(string.Characters16(), length);
}

bool SpaceSplitString::Data::ContainsAll(Data& other) {
  if (this == &other)
    return true;

  size_t this_size = vector_.size();
  size_t other_size = other.vector_.size();
  for (size_t i = 0; i < other_size; ++i) {
    const AtomicString& name = other.vector_[i];
    size_t j;
    for (j = 0; j < this_size; ++j) {
      if (vector_[j] == name)
        break;
    }
    if (j == this_size)
      return false;
  }
  return true;
}

void SpaceSplitString::Data::Add(const AtomicString& string) {
  DCHECK(HasOneRef());
  DCHECK(!Contains(string));
  vector_.push_back(string);
}

void SpaceSplitString::Data::Remove(unsigned index) {
  DCHECK(HasOneRef());
  vector_.erase(index);
}

void SpaceSplitString::Add(const AtomicString& string) {
  // FIXME: add() does not allow duplicates but createVector() does.
  if (Contains(string))
    return;
  EnsureUnique();
  if (data_)
    data_->Add(string);
}

bool SpaceSplitString::Remove(const AtomicString& string) {
  if (!data_)
    return false;
  unsigned i = 0;
  bool changed = false;
  while (i < data_->size()) {
    if ((*data_)[i] == string) {
      if (!changed)
        EnsureUnique();
      data_->Remove(i);
      changed = true;
      continue;
    }
    ++i;
  }
  return changed;
}

SpaceSplitString::DataMap& SpaceSplitString::SharedDataMap() {
  DEFINE_STATIC_LOCAL(DataMap, map, ());
  return map;
}

void SpaceSplitString::Set(const AtomicString& input_string,
                           CaseFolding case_folding) {
  if (input_string.IsNull()) {
    Clear();
    return;
  }

  if (case_folding == kShouldFoldCase &&
      HasNonASCIIOrUpper(input_string.GetString())) {
    String string(input_string.GetString());
    string = string.FoldCase();
    data_ = Data::Create(AtomicString(string));
  } else {
    data_ = Data::Create(input_string);
  }
}

SpaceSplitString::Data::~Data() {
  if (!key_string_.IsNull())
    SharedDataMap().erase(key_string_);
}

PassRefPtr<SpaceSplitString::Data> SpaceSplitString::Data::Create(
    const AtomicString& string) {
  Data*& data = SharedDataMap().insert(string, nullptr).stored_value->value;
  if (!data) {
    data = new Data(string);
    return AdoptRef(data);
  }
  return data;
}

PassRefPtr<SpaceSplitString::Data> SpaceSplitString::Data::CreateUnique(
    const Data& other) {
  return AdoptRef(new SpaceSplitString::Data(other));
}

SpaceSplitString::Data::Data(const AtomicString& string) : key_string_(string) {
  DCHECK(!string.IsNull());
  CreateVector(string);
}

SpaceSplitString::Data::Data(const SpaceSplitString::Data& other)
    : RefCounted<Data>(), vector_(other.vector_) {
  // Note that we don't copy m_keyString to indicate to the destructor that
  // there's nothing to be removed from the sharedDataMap().
}

}  // namespace blink
