/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/dom/AXObjectCache.h"

#include <memory>
#include "platform/wtf/Assertions.h"
#include "platform/wtf/PtrUtil.h"
#include "public/web/WebAXEnums.h"

namespace blink {

AXObjectCache::AXObjectCacheCreateFunction AXObjectCache::create_function_ =
    nullptr;

void AXObjectCache::Init(AXObjectCacheCreateFunction function) {
  DCHECK(!create_function_);
  create_function_ = function;
}

AXObjectCache* AXObjectCache::Create(Document& document) {
  DCHECK(create_function_);
  return create_function_(document);
}

AXObjectCache::AXObjectCache() {}

AXObjectCache::~AXObjectCache() {}

std::unique_ptr<ScopedAXObjectCache> ScopedAXObjectCache::Create(
    Document& document) {
  return WTF::WrapUnique(new ScopedAXObjectCache(document));
}

ScopedAXObjectCache::ScopedAXObjectCache(Document& document)
    : document_(&document) {
  if (!document_->AxObjectCache())
    cache_ = AXObjectCache::Create(*document_);
}

ScopedAXObjectCache::~ScopedAXObjectCache() {
  if (cache_)
    cache_->Dispose();
}

AXObjectCache* ScopedAXObjectCache::Get() {
  if (cache_)
    return cache_.Get();
  AXObjectCache* cache = document_->AxObjectCache();
  DCHECK(cache);
  return cache;
}

STATIC_ASSERT_ENUM(kWebAXEventActiveDescendantChanged,
                   AXObjectCache::kAXActiveDescendantChanged);
STATIC_ASSERT_ENUM(kWebAXEventAlert, AXObjectCache::kAXAlert);
STATIC_ASSERT_ENUM(kWebAXEventAriaAttributeChanged,
                   AXObjectCache::kAXAriaAttributeChanged);
STATIC_ASSERT_ENUM(kWebAXEventAutocorrectionOccured,
                   AXObjectCache::kAXAutocorrectionOccured);
STATIC_ASSERT_ENUM(kWebAXEventBlur, AXObjectCache::kAXBlur);
STATIC_ASSERT_ENUM(kWebAXEventCheckedStateChanged,
                   AXObjectCache::kAXCheckedStateChanged);
STATIC_ASSERT_ENUM(kWebAXEventChildrenChanged,
                   AXObjectCache::kAXChildrenChanged);
STATIC_ASSERT_ENUM(kWebAXEventClicked, AXObjectCache::kAXClicked);
STATIC_ASSERT_ENUM(kWebAXEventDocumentSelectionChanged,
                   AXObjectCache::kAXDocumentSelectionChanged);
STATIC_ASSERT_ENUM(kWebAXEventExpandedChanged,
                   AXObjectCache::kAXExpandedChanged);
STATIC_ASSERT_ENUM(kWebAXEventFocus, AXObjectCache::kAXFocusedUIElementChanged);
STATIC_ASSERT_ENUM(kWebAXEventHide, AXObjectCache::kAXHide);
STATIC_ASSERT_ENUM(kWebAXEventHover, AXObjectCache::kAXHover);
STATIC_ASSERT_ENUM(kWebAXEventInvalidStatusChanged,
                   AXObjectCache::kAXInvalidStatusChanged);
STATIC_ASSERT_ENUM(kWebAXEventLayoutComplete, AXObjectCache::kAXLayoutComplete);
STATIC_ASSERT_ENUM(kWebAXEventLiveRegionChanged,
                   AXObjectCache::kAXLiveRegionChanged);
STATIC_ASSERT_ENUM(kWebAXEventLoadComplete, AXObjectCache::kAXLoadComplete);
STATIC_ASSERT_ENUM(kWebAXEventLocationChanged,
                   AXObjectCache::kAXLocationChanged);
STATIC_ASSERT_ENUM(kWebAXEventMenuListItemSelected,
                   AXObjectCache::kAXMenuListItemSelected);
STATIC_ASSERT_ENUM(kWebAXEventMenuListItemUnselected,
                   AXObjectCache::kAXMenuListItemUnselected);
STATIC_ASSERT_ENUM(kWebAXEventMenuListValueChanged,
                   AXObjectCache::kAXMenuListValueChanged);
STATIC_ASSERT_ENUM(kWebAXEventRowCollapsed, AXObjectCache::kAXRowCollapsed);
STATIC_ASSERT_ENUM(kWebAXEventRowCountChanged,
                   AXObjectCache::kAXRowCountChanged);
STATIC_ASSERT_ENUM(kWebAXEventRowExpanded, AXObjectCache::kAXRowExpanded);
STATIC_ASSERT_ENUM(kWebAXEventScrollPositionChanged,
                   AXObjectCache::kAXScrollPositionChanged);
STATIC_ASSERT_ENUM(kWebAXEventScrolledToAnchor,
                   AXObjectCache::kAXScrolledToAnchor);
STATIC_ASSERT_ENUM(kWebAXEventSelectedChildrenChanged,
                   AXObjectCache::kAXSelectedChildrenChanged);
STATIC_ASSERT_ENUM(kWebAXEventSelectedTextChanged,
                   AXObjectCache::kAXSelectedTextChanged);
STATIC_ASSERT_ENUM(kWebAXEventShow, AXObjectCache::kAXShow);
STATIC_ASSERT_ENUM(kWebAXEventTextChanged, AXObjectCache::kAXTextChanged);
STATIC_ASSERT_ENUM(kWebAXEventTextInserted, AXObjectCache::kAXTextInserted);
STATIC_ASSERT_ENUM(kWebAXEventTextRemoved, AXObjectCache::kAXTextRemoved);
STATIC_ASSERT_ENUM(kWebAXEventValueChanged, AXObjectCache::kAXValueChanged);

#undef STATIC_ASSERT_ENUM

}  // namespace blink
