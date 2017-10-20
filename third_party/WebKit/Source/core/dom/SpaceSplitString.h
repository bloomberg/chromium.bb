/*
 * Copyright (C) 2007, 2008, 2010, 2011, 2012 Apple Inc. All rights reserved.
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
 *
 */

#ifndef SpaceSplitString_h
#define SpaceSplitString_h

#include "core/CoreExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class CORE_EXPORT SpaceSplitString {
  USING_FAST_MALLOC(SpaceSplitString);

 public:
  SpaceSplitString() {}
  explicit SpaceSplitString(const AtomicString& string) { Set(string); }

  bool operator!=(const SpaceSplitString& other) const {
    return data_ != other.data_;
  }

  void Set(const AtomicString&);
  void Clear() { data_ = nullptr; }

  bool Contains(const AtomicString& string) const {
    return data_ && data_->Contains(string);
  }
  bool ContainsAll(const SpaceSplitString& names) const {
    return !names.data_ || (data_ && data_->ContainsAll(*names.data_));
  }
  void Add(const AtomicString&);
  bool Remove(const AtomicString&);
  void Remove(size_t index);
  void ReplaceAt(size_t index, const AtomicString&);

  size_t size() const { return data_ ? data_->size() : 0; }
  bool IsNull() const { return !data_; }
  const AtomicString& operator[](size_t i) const { return (*data_)[i]; }

 private:
  class Data : public RefCounted<Data> {
   public:
    static scoped_refptr<Data> Create(const AtomicString&);
    static scoped_refptr<Data> CreateUnique(const Data&);

    ~Data();

    bool Contains(const AtomicString& string) {
      for (const auto& item : vector_) {
        if (item == string)
          return true;
      }
      return false;
    }

    bool ContainsAll(Data&);

    void Add(const AtomicString&);
    void Remove(unsigned index);

    bool IsUnique() const { return key_string_.IsNull(); }
    size_t size() const { return vector_.size(); }
    const AtomicString& operator[](size_t i) const { return vector_[i]; }
    AtomicString& operator[](size_t i) { return vector_[i]; }

   private:
    explicit Data(const AtomicString&);
    explicit Data(const Data&);

    void CreateVector(const AtomicString&);
    template <typename CharacterType>
    inline void CreateVector(const AtomicString&,
                             const CharacterType*,
                             unsigned);

    AtomicString key_string_;
    Vector<AtomicString, 4> vector_;
  };
  typedef HashMap<AtomicString, Data*> DataMap;

  static DataMap& SharedDataMap();

  void EnsureUnique() {
    if (data_ && !data_->IsUnique())
      data_ = Data::CreateUnique(*data_);
  }

  scoped_refptr<Data> data_;
};

}  // namespace blink

#endif  // SpaceSplitString_h
