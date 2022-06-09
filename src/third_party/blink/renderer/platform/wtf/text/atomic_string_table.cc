// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/atomic_string_table.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/utf8.h"

namespace WTF {

namespace {

// TODO(ajwong): consider replacing with a span in the future.
template <typename CharacterType>
struct HashTranslatorCharBuffer {
  const CharacterType* s;
  unsigned length;
};

typedef HashTranslatorCharBuffer<UChar> UCharBuffer;
struct UCharBufferTranslator {
  static unsigned GetHash(const UCharBuffer& buf) {
    return StringHasher::ComputeHashAndMaskTop8Bits(buf.s, buf.length);
  }

  static bool Equal(StringImpl* const& str, const UCharBuffer& buf) {
    return WTF::Equal(str, buf.s, buf.length);
  }

  static void Translate(StringImpl*& location,
                        const UCharBuffer& buf,
                        unsigned hash) {
    auto string = StringImpl::Create8BitIfPossible(buf.s, buf.length);
    location = string.release();
    location->SetHash(hash);
    location->SetIsAtomic();
  }
};

struct HashAndUTF8Characters {
  unsigned hash;
  const char* characters;
  unsigned length;
  unsigned utf16_length;
};

struct HashAndUTF8CharactersTranslator {
  static unsigned GetHash(const HashAndUTF8Characters& buffer) {
    return buffer.hash;
  }

  static bool Equal(StringImpl* const& string,
                    const HashAndUTF8Characters& buffer) {
    if (buffer.utf16_length != string->length())
      return false;

    // If buffer contains only ASCII characters UTF-8 and UTF16 length are the
    // same.
    if (buffer.utf16_length != buffer.length) {
      if (string->Is8Bit()) {
        const LChar* characters8 = string->Characters8();
        return unicode::EqualLatin1WithUTF8(
            characters8, characters8 + string->length(), buffer.characters,
            buffer.characters + buffer.length);
      }
      const UChar* characters16 = string->Characters16();
      return unicode::EqualUTF16WithUTF8(
          characters16, characters16 + string->length(), buffer.characters,
          buffer.characters + buffer.length);
    }

    if (string->Is8Bit()) {
      const LChar* string_characters = string->Characters8();

      for (unsigned i = 0; i < buffer.length; ++i) {
        DCHECK(IsASCII(buffer.characters[i]));
        if (string_characters[i] != buffer.characters[i])
          return false;
      }

      return true;
    }

    const UChar* string_characters = string->Characters16();

    for (unsigned i = 0; i < buffer.length; ++i) {
      DCHECK(IsASCII(buffer.characters[i]));
      if (string_characters[i] != buffer.characters[i])
        return false;
    }

    return true;
  }

  static void Translate(StringImpl*& location,
                        const HashAndUTF8Characters& buffer,
                        unsigned hash) {
    scoped_refptr<StringImpl> new_string;
    // If buffer contains only ASCII characters, the UTF-8 and UTF-16 lengths
    // are the same.
    bool is_all_ascii = buffer.utf16_length == buffer.length;
    if (!is_all_ascii) {
      UChar* target;
      new_string = StringImpl::CreateUninitialized(buffer.utf16_length, target);

      const char* source = buffer.characters;
      if (unicode::ConvertUTF8ToUTF16(&source, source + buffer.length, &target,
                                      target + buffer.utf16_length,
                                      &is_all_ascii) != unicode::kConversionOK)
        NOTREACHED();
    } else {
      new_string = StringImpl::Create(buffer.characters, buffer.length);
    }
    location = new_string.release();
    location->SetHash(hash);
    location->SetIsAtomic();
  }
};

struct StringViewLookupTranslator {
  static unsigned GetHash(const StringView& buf) {
    StringImpl* shared_impl = buf.SharedImpl();
    if (LIKELY(shared_impl))
      return shared_impl->GetHash();

    if (buf.Is8Bit()) {
      return StringHasher::ComputeHashAndMaskTop8Bits(buf.Characters8(),
                                                      buf.length());
    } else {
      return StringHasher::ComputeHashAndMaskTop8Bits(buf.Characters16(),
                                                      buf.length());
    }
  }

  static bool Equal(StringImpl* const& str, const StringView& buf) {
    return *str == buf;
  }
};

// Allows lookups of the ASCII-lowercase version of a string without actually
// allocating memory to store it. Instead, the translator computes the results
// of hash and equality computations as if we had done so.
struct LowercaseStringViewLookupTranslator {
  template <typename CharType>
  static UChar ToASCIILowerUChar(CharType ch) {
    return ToASCIILower(ch);
  }

  // Computes the hash that |query| would have if it were first converted to
  // ASCII lowercase.
  static unsigned GetHash(const StringView& query) {
    // If possible, use cached hash if the string is lowercased.
    StringImpl* shared_impl = query.SharedImpl();
    if (LIKELY(shared_impl && query.IsLowerASCII()))
      return shared_impl->GetHash();

    if (query.Is8Bit()) {
      return StringHasher::ComputeHashAndMaskTop8Bits<LChar,
                                                      ToASCIILowerUChar<LChar>>(
          query.Characters8(), query.length());
    } else {
      return StringHasher::ComputeHashAndMaskTop8Bits<UChar,
                                                      ToASCIILowerUChar<UChar>>(
          query.Characters16(), query.length());
    }
  }

  // Returns true if the hashtable |bucket| contains a string which is the ASCII
  // lowercase version of |query|.
  static bool Equal(StringImpl* const& bucket, const StringView& query) {
    // This is similar to EqualIgnoringASCIICase, but not the same.
    // In particular, it validates that |bucket| is a lowercase version of
    // |query|.
    //
    // Unlike EqualIgnoringASCIICase, it returns false if they are equal
    // ignoring ASCII case but |bucket| contains an uppercase ASCII character.
    //
    // However, similar optimizations are used here as there, so these should
    // have generally similar correctness and performance constraints.
    if (bucket->length() != query.length())
      return false;
    if (bucket->Bytes() == query.Bytes() && bucket->Is8Bit() == query.Is8Bit())
      return query.IsLowerASCII();
    return WTF::VisitCharacters(*bucket, [&](const auto* bch, wtf_size_t) {
      return WTF::VisitCharacters(query, [&](const auto* qch, wtf_size_t len) {
        for (wtf_size_t i = 0; i < len; ++i) {
          if (bch[i] != ToASCIILower(qch[i]))
            return false;
        }
        return true;
      });
    });
  }
};

}  // namespace

AtomicStringTable::AtomicStringTable() {
  for (StringImpl* string : StringImpl::AllStaticStrings().Values())
    Add(string);
}

AtomicStringTable::~AtomicStringTable() {
  for (StringImpl* string : table_) {
    if (!string->IsStatic()) {
      DCHECK(string->IsAtomic());
      string->UnsetIsAtomic();
    }
  }
}

void AtomicStringTable::ReserveCapacity(unsigned size) {
  table_.ReserveCapacityForSize(size);
}

template <typename T, typename HashTranslator>
scoped_refptr<StringImpl> AtomicStringTable::AddToStringTable(const T& value) {
  HashSet<StringImpl*>::AddResult add_result =
      table_.AddWithTranslator<HashTranslator>(value);

  // If the string is newly-translated, then we need to adopt it.
  // The boolean in the pair tells us if that is so.
  return add_result.is_new_entry ? base::AdoptRef(*add_result.stored_value)
                                 : *add_result.stored_value;
}

scoped_refptr<StringImpl> AtomicStringTable::Add(const UChar* s,
                                                 unsigned length) {
  if (!s)
    return nullptr;

  if (!length)
    return StringImpl::empty_;

  UCharBuffer buffer = {s, length};
  return AddToStringTable<UCharBuffer, UCharBufferTranslator>(buffer);
}

typedef HashTranslatorCharBuffer<LChar> LCharBuffer;
struct LCharBufferTranslator {
  static unsigned GetHash(const LCharBuffer& buf) {
    return StringHasher::ComputeHashAndMaskTop8Bits(buf.s, buf.length);
  }

  static bool Equal(StringImpl* const& str, const LCharBuffer& buf) {
    return WTF::Equal(str, buf.s, buf.length);
  }

  static void Translate(StringImpl*& location,
                        const LCharBuffer& buf,
                        unsigned hash) {
    auto string = StringImpl::Create(buf.s, buf.length);
    location = string.release();
    location->SetHash(hash);
    location->SetIsAtomic();
  }
};

scoped_refptr<StringImpl> AtomicStringTable::Add(const LChar* s,
                                                 unsigned length) {
  if (!s)
    return nullptr;

  if (!length)
    return StringImpl::empty_;

  LCharBuffer buffer = {s, length};
  return AddToStringTable<LCharBuffer, LCharBufferTranslator>(buffer);
}

StringImpl* AtomicStringTable::Add(StringImpl* string) {
  if (!string->length())
    return StringImpl::empty_;

  StringImpl* result = *table_.insert(string).stored_value;

  if (!result->IsAtomic())
    result->SetIsAtomic();

  DCHECK(!string->IsStatic() || result->IsStatic());
  return result;
}

scoped_refptr<StringImpl> AtomicStringTable::AddUTF8(
    const char* characters_start,
    const char* characters_end) {
  HashAndUTF8Characters buffer;
  buffer.characters = characters_start;
  buffer.hash = unicode::CalculateStringHashAndLengthFromUTF8MaskingTop8Bits(
      characters_start, characters_end, buffer.length, buffer.utf16_length);

  if (!buffer.hash)
    return nullptr;

  return AddToStringTable<HashAndUTF8Characters,
                          HashAndUTF8CharactersTranslator>(buffer);
}

AtomicStringTable::WeakResult AtomicStringTable::WeakFindSlow(
    StringImpl* string) {
  DCHECK(string->length());
  const auto& it = table_.find(string);
  if (it == table_.end())
    return WeakResult();
  return WeakResult(*it);
}

AtomicStringTable::WeakResult AtomicStringTable::WeakFindSlow(
    const StringView& string) {
  DCHECK(string.length());
  const auto& it = table_.Find<StringViewLookupTranslator>(string);
  if (it == table_.end())
    return WeakResult();
  return WeakResult(*it);
}

AtomicStringTable::WeakResult AtomicStringTable::WeakFindLowercasedSlow(
    const StringView& string) {
  DCHECK(string.length());
  const auto& it = table_.Find<LowercaseStringViewLookupTranslator>(string);
  if (it == table_.end())
    return WeakResult();
  DCHECK(StringView(*it).IsLowerASCII());
  DCHECK(EqualIgnoringASCIICase(*it, string));
  return WeakResult(*it);
}

AtomicStringTable::WeakResult AtomicStringTable::WeakFind(const LChar* chars,
                                                          unsigned length) {
  if (!chars)
    return WeakResult();

  // Mirror the empty logic in Add().
  if (!length)
    return WeakResult(StringImpl::empty_);

  LCharBuffer buffer = {chars, length};
  const auto& it = table_.Find<LCharBufferTranslator>(buffer);
  if (it == table_.end())
    return WeakResult();

  return WeakResult(*it);
}

AtomicStringTable::WeakResult AtomicStringTable::WeakFind(const UChar* chars,
                                                          unsigned length) {
  if (!chars)
    return WeakResult();

  // Mirror the empty logic in Add().
  if (!length)
    return WeakResult(StringImpl::empty_);

  UCharBuffer buffer = {chars, length};
  const auto& it = table_.Find<UCharBufferTranslator>(buffer);
  if (it == table_.end())
    return WeakResult();

  return WeakResult(*it);
}

void AtomicStringTable::Remove(StringImpl* string) {
  DCHECK(string->IsAtomic());
  auto iterator = table_.find(string);
  CHECK_NE(iterator, table_.end());
  table_.erase(iterator);
}

}  // namespace WTF
