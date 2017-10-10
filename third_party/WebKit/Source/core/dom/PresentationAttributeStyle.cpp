/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/dom/PresentationAttributeStyle.h"

#include <algorithm>
#include "core/css/StylePropertySet.h"
#include "core/dom/Attribute.h"
#include "core/dom/Element.h"
#include "core/html/forms/HTMLInputElement.h"
#include "platform/Timer.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/wtf/HashFunctions.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/text/CString.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"

namespace blink {

using namespace HTMLNames;

struct PresentationAttributeCacheKey {
  PresentationAttributeCacheKey() : tag_name(nullptr) {}
  StringImpl* tag_name;
  Vector<std::pair<StringImpl*, AtomicString>, 3> attributes_and_values;
};

static bool operator!=(const PresentationAttributeCacheKey& a,
                       const PresentationAttributeCacheKey& b) {
  if (a.tag_name != b.tag_name)
    return true;
  return a.attributes_and_values != b.attributes_and_values;
}

struct PresentationAttributeCacheEntry final
    : public GarbageCollectedFinalized<PresentationAttributeCacheEntry> {
 public:
  DEFINE_INLINE_TRACE() { visitor->Trace(value); }

  PresentationAttributeCacheKey key;
  Member<StylePropertySet> value;
};

using PresentationAttributeCache =
    HeapHashMap<unsigned,
                Member<PresentationAttributeCacheEntry>,
                AlreadyHashed>;
static PresentationAttributeCache& GetPresentationAttributeCache() {
  DEFINE_STATIC_LOCAL(PresentationAttributeCache, cache,
                      (new PresentationAttributeCache));
  return cache;
}

// This is a singleton (held via DEFINE_STATIC_LOCAL).
// Thus it is appropriate to use the main thread's timer task runner, rather
// than one associated with a particular frame.
class PresentationAttributeCacheCleaner {
  WTF_MAKE_NONCOPYABLE(PresentationAttributeCacheCleaner);
  USING_FAST_MALLOC(PresentationAttributeCacheCleaner);

 public:
  PresentationAttributeCacheCleaner()
      : hit_count_(0),
        clean_timer_(
            Platform::Current()->MainThread()->Scheduler()->TimerTaskRunner(),
            this,
            &PresentationAttributeCacheCleaner::CleanCache) {}

  void DidHitPresentationAttributeCache() {
    if (GetPresentationAttributeCache().size() <
        kMinimumPresentationAttributeCacheSizeForCleaning)
      return;

    hit_count_++;

    if (!clean_timer_.IsActive())
      clean_timer_.StartOneShot(kPresentationAttributeCacheCleanTimeInSeconds,
                                BLINK_FROM_HERE);
  }

 private:
  static const unsigned kPresentationAttributeCacheCleanTimeInSeconds = 60;
  static const unsigned kMinimumPresentationAttributeCacheSizeForCleaning = 100;
  static const unsigned kMinimumPresentationAttributeCacheHitCountPerMinute =
      (100 * kPresentationAttributeCacheCleanTimeInSeconds) / 60;

  void CleanCache(TimerBase* timer) {
    DCHECK_EQ(timer, &clean_timer_);
    unsigned hit_count = hit_count_;
    hit_count_ = 0;
    if (hit_count > kMinimumPresentationAttributeCacheHitCountPerMinute)
      return;
    GetPresentationAttributeCache().clear();
  }

  unsigned hit_count_;
  TaskRunnerTimer<PresentationAttributeCacheCleaner> clean_timer_;
};

static bool AttributeNameSort(const std::pair<StringImpl*, AtomicString>& p1,
                              const std::pair<StringImpl*, AtomicString>& p2) {
  // Sort based on the attribute name pointers. It doesn't matter what the order
  // is as long as it is always the same.
  return p1.first < p2.first;
}

static void MakePresentationAttributeCacheKey(
    Element& element,
    PresentationAttributeCacheKey& result) {
  // FIXME: Enable for SVG.
  if (!element.IsHTMLElement())
    return;
  // Interpretation of the size attributes on <input> depends on the type
  // attribute.
  if (IsHTMLInputElement(element))
    return;
  AttributeCollection attributes = element.AttributesWithoutUpdate();
  for (const Attribute& attr : attributes) {
    if (!element.IsPresentationAttribute(attr.GetName()))
      continue;
    if (!attr.NamespaceURI().IsNull())
      return;
    // FIXME: Background URL may depend on the base URL and can't be shared.
    // Disallow caching.
    if (attr.GetName() == backgroundAttr)
      return;
    result.attributes_and_values.push_back(
        std::make_pair(attr.LocalName().Impl(), attr.Value()));
  }
  if (result.attributes_and_values.IsEmpty())
    return;
  // Attribute order doesn't matter. Sort for easy equality comparison.
  std::sort(result.attributes_and_values.begin(),
            result.attributes_and_values.end(), AttributeNameSort);
  // The cache key is non-null when the tagName is set.
  result.tag_name = element.localName().Impl();
}

static unsigned ComputePresentationAttributeCacheHash(
    const PresentationAttributeCacheKey& key) {
  if (!key.tag_name)
    return 0;
  DCHECK(key.attributes_and_values.size());
  unsigned attribute_hash = StringHasher::HashMemory(
      key.attributes_and_values.data(),
      key.attributes_and_values.size() * sizeof(key.attributes_and_values[0]));
  return WTF::HashInts(key.tag_name->ExistingHash(), attribute_hash);
}

StylePropertySet* ComputePresentationAttributeStyle(Element& element) {
  DEFINE_STATIC_LOCAL(PresentationAttributeCacheCleaner, cache_cleaner, ());

  DCHECK(element.IsStyledElement());

  PresentationAttributeCacheKey cache_key;
  MakePresentationAttributeCacheKey(element, cache_key);

  unsigned cache_hash = ComputePresentationAttributeCacheHash(cache_key);

  PresentationAttributeCache::ValueType* cache_value;
  if (cache_hash) {
    cache_value = GetPresentationAttributeCache()
                      .insert(cache_hash, nullptr)
                      .stored_value;
    if (cache_value->value && cache_value->value->key != cache_key)
      cache_hash = 0;
  } else {
    cache_value = nullptr;
  }

  StylePropertySet* style = nullptr;
  if (cache_hash && cache_value->value) {
    style = cache_value->value->value;
    cache_cleaner.DidHitPresentationAttributeCache();
  } else {
    style = MutableStylePropertySet::Create(
        element.IsSVGElement() ? kSVGAttributeMode : kHTMLStandardMode);
    AttributeCollection attributes = element.AttributesWithoutUpdate();
    for (const Attribute& attr : attributes)
      element.CollectStyleForPresentationAttribute(
          attr.GetName(), attr.Value(), ToMutableStylePropertySet(style));
  }

  if (!cache_hash || cache_value->value)
    return style;

  PresentationAttributeCacheEntry* new_entry =
      new PresentationAttributeCacheEntry;
  new_entry->key = cache_key;
  new_entry->value = style;

  static const unsigned kPresentationAttributeCacheMaximumSize = 4096;
  if (GetPresentationAttributeCache().size() >
      kPresentationAttributeCacheMaximumSize) {
    // FIXME: Discarding the entire cache when it gets too big is probably bad
    // since it creates a perf "cliff". Perhaps we should use an LRU?
    GetPresentationAttributeCache().clear();
    GetPresentationAttributeCache().Set(cache_hash, new_entry);
  } else {
    cache_value->value = new_entry;
  }

  return style;
}

}  // namespace blink
