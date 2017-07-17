/*
 * Copyright (C) 2006, 2007, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
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
 */

#include "platform/wtf/text/TextEncodingRegistry.h"

#include "platform/wtf/ASCIICType.h"
#include "platform/wtf/Atomics.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/StringExtras.h"
#include "platform/wtf/ThreadingPrimitives.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/TextCodecICU.h"
#include "platform/wtf/text/TextCodecLatin1.h"
#include "platform/wtf/text/TextCodecReplacement.h"
#include "platform/wtf/text/TextCodecUTF16.h"
#include "platform/wtf/text/TextCodecUTF8.h"
#include "platform/wtf/text/TextCodecUserDefined.h"
#include "platform/wtf/text/TextEncoding.h"
#include <memory>

namespace WTF {

const size_t kMaxEncodingNameLength = 63;

// Hash for all-ASCII strings that does case folding.
struct TextEncodingNameHash {
  static bool Equal(const char* s1, const char* s2) {
    char c1;
    char c2;
    do {
      c1 = *s1++;
      c2 = *s2++;
      if (ToASCIILower(c1) != ToASCIILower(c2))
        return false;
    } while (c1 && c2);
    return !c1 && !c2;
  }

  // This algorithm is the one-at-a-time hash from:
  // http://burtleburtle.net/bob/hash/hashfaq.html
  // http://burtleburtle.net/bob/hash/doobs.html
  static unsigned GetHash(const char* s) {
    unsigned h = WTF::kStringHashingStartValue;
    for (;;) {
      char c = *s++;
      if (!c) {
        h += (h << 3);
        h ^= (h >> 11);
        h += (h << 15);
        return h;
      }
      h += ToASCIILower(c);
      h += (h << 10);
      h ^= (h >> 6);
    }
  }

  static const bool safe_to_compare_to_empty_or_deleted = false;
};

struct TextCodecFactory {
  NewTextCodecFunction function;
  const void* additional_data;
  TextCodecFactory(NewTextCodecFunction f = 0, const void* d = 0)
      : function(f), additional_data(d) {}
};

typedef HashMap<const char*, const char*, TextEncodingNameHash>
    TextEncodingNameMap;
typedef HashMap<const char*, TextCodecFactory> TextCodecMap;

static Mutex& EncodingRegistryMutex() {
  // We don't have to use AtomicallyInitializedStatic here because
  // this function is called on the main thread for any page before
  // it is used in worker threads.
  DEFINE_STATIC_LOCAL(Mutex, mutex, ());
  return mutex;
}

static TextEncodingNameMap* g_text_encoding_name_map;
static TextCodecMap* g_text_codec_map;

namespace {
static unsigned g_did_extend_text_codec_maps = 0;

ALWAYS_INLINE unsigned AtomicDidExtendTextCodecMaps() {
  return AcquireLoad(&g_did_extend_text_codec_maps);
}

ALWAYS_INLINE void AtomicSetDidExtendTextCodecMaps() {
  ReleaseStore(&g_did_extend_text_codec_maps, 1);
}
}  // namespace

static const char kTextEncodingNameBlacklist[][6] = {"UTF-7"};

#if ERROR_DISABLED

static inline void checkExistingName(const char*, const char*) {}

#else

static void CheckExistingName(const char* alias, const char* atomic_name) {
  const char* old_atomic_name = g_text_encoding_name_map->at(alias);
  if (!old_atomic_name)
    return;
  if (old_atomic_name == atomic_name)
    return;
  // Keep the warning silent about one case where we know this will happen.
  if (strcmp(alias, "ISO-8859-8-I") == 0 &&
      strcmp(old_atomic_name, "ISO-8859-8-I") == 0 &&
      strcasecmp(atomic_name, "iso-8859-8") == 0)
    return;
  LOG(ERROR) << "alias " << alias << " maps to " << old_atomic_name
             << " already, but someone is trying to make it map to "
             << atomic_name;
}

#endif

static bool IsUndesiredAlias(const char* alias) {
  // Reject aliases with version numbers that are supported by some back-ends
  // (such as "ISO_2022,locale=ja,version=0" in ICU).
  for (const char* p = alias; *p; ++p) {
    if (*p == ',')
      return true;
  }
  // 8859_1 is known to (at least) ICU, but other browsers don't support this
  // name - and having it caused a compatibility
  // problem, see bug 43554.
  if (0 == strcmp(alias, "8859_1"))
    return true;
  return false;
}

static void AddToTextEncodingNameMap(const char* alias, const char* name) {
  DCHECK_LE(strlen(alias), kMaxEncodingNameLength);
  if (IsUndesiredAlias(alias))
    return;
  const char* atomic_name = g_text_encoding_name_map->at(name);
  DCHECK(strcmp(alias, name) == 0 || atomic_name);
  if (!atomic_name)
    atomic_name = name;
  CheckExistingName(alias, atomic_name);
  g_text_encoding_name_map->insert(alias, atomic_name);
}

static void AddToTextCodecMap(const char* name,
                              NewTextCodecFunction function,
                              const void* additional_data) {
  const char* atomic_name = g_text_encoding_name_map->at(name);
  DCHECK(atomic_name);
  g_text_codec_map->insert(atomic_name,
                           TextCodecFactory(function, additional_data));
}

static void PruneBlacklistedCodecs() {
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(kTextEncodingNameBlacklist); ++i) {
    const char* atomic_name =
        g_text_encoding_name_map->at(kTextEncodingNameBlacklist[i]);
    if (!atomic_name)
      continue;

    Vector<const char*> names;
    TextEncodingNameMap::const_iterator it = g_text_encoding_name_map->begin();
    TextEncodingNameMap::const_iterator end = g_text_encoding_name_map->end();
    for (; it != end; ++it) {
      if (it->value == atomic_name)
        names.push_back(it->key);
    }

    g_text_encoding_name_map->RemoveAll(names);

    g_text_codec_map->erase(atomic_name);
  }
}

static void BuildBaseTextCodecMaps() {
  DCHECK(IsMainThread());
  DCHECK(!g_text_codec_map);
  DCHECK(!g_text_encoding_name_map);

  g_text_codec_map = new TextCodecMap;
  g_text_encoding_name_map = new TextEncodingNameMap;

  TextCodecLatin1::RegisterEncodingNames(AddToTextEncodingNameMap);
  TextCodecLatin1::RegisterCodecs(AddToTextCodecMap);

  TextCodecUTF8::RegisterEncodingNames(AddToTextEncodingNameMap);
  TextCodecUTF8::RegisterCodecs(AddToTextCodecMap);

  TextCodecUTF16::RegisterEncodingNames(AddToTextEncodingNameMap);
  TextCodecUTF16::RegisterCodecs(AddToTextCodecMap);

  TextCodecUserDefined::RegisterEncodingNames(AddToTextEncodingNameMap);
  TextCodecUserDefined::RegisterCodecs(AddToTextCodecMap);
}

static void ExtendTextCodecMaps() {
  TextCodecReplacement::RegisterEncodingNames(AddToTextEncodingNameMap);
  TextCodecReplacement::RegisterCodecs(AddToTextCodecMap);

  TextCodecICU::RegisterEncodingNames(AddToTextEncodingNameMap);
  TextCodecICU::RegisterCodecs(AddToTextCodecMap);

  PruneBlacklistedCodecs();
}

std::unique_ptr<TextCodec> NewTextCodec(const TextEncoding& encoding) {
  MutexLocker lock(EncodingRegistryMutex());

  DCHECK(g_text_codec_map);
  TextCodecFactory factory = g_text_codec_map->at(encoding.GetName());
  DCHECK(factory.function);
  return factory.function(encoding, factory.additional_data);
}

const char* AtomicCanonicalTextEncodingName(const char* name) {
  if (!name || !name[0])
    return 0;
  if (!g_text_encoding_name_map)
    BuildBaseTextCodecMaps();

  MutexLocker lock(EncodingRegistryMutex());

  if (const char* atomic_name = g_text_encoding_name_map->at(name))
    return atomic_name;
  if (AtomicDidExtendTextCodecMaps())
    return 0;
  ExtendTextCodecMaps();
  AtomicSetDidExtendTextCodecMaps();
  return g_text_encoding_name_map->at(name);
}

template <typename CharacterType>
const char* AtomicCanonicalTextEncodingName(const CharacterType* characters,
                                            size_t length) {
  char buffer[kMaxEncodingNameLength + 1];
  size_t j = 0;
  for (size_t i = 0; i < length; ++i) {
    char c = static_cast<char>(characters[i]);
    if (j == kMaxEncodingNameLength || c != characters[i])
      return 0;
    buffer[j++] = c;
  }
  buffer[j] = 0;
  return AtomicCanonicalTextEncodingName(buffer);
}

const char* AtomicCanonicalTextEncodingName(const String& alias) {
  if (!alias.length())
    return 0;

  if (alias.Contains('\0'))
    return 0;

  if (alias.Is8Bit())
    return AtomicCanonicalTextEncodingName<LChar>(alias.Characters8(),
                                                  alias.length());

  return AtomicCanonicalTextEncodingName<UChar>(alias.Characters16(),
                                                alias.length());
}

bool NoExtendedTextEncodingNameUsed() {
  return !AtomicDidExtendTextCodecMaps();
}

#ifndef NDEBUG
void DumpTextEncodingNameMap() {
  unsigned size = g_text_encoding_name_map->size();
  fprintf(stderr, "Dumping %u entries in WTF::TextEncodingNameMap...\n", size);

  MutexLocker lock(EncodingRegistryMutex());

  TextEncodingNameMap::const_iterator it = g_text_encoding_name_map->begin();
  TextEncodingNameMap::const_iterator end = g_text_encoding_name_map->end();
  for (; it != end; ++it)
    fprintf(stderr, "'%s' => '%s'\n", it->key, it->value);
}
#endif

}  // namespace WTF
