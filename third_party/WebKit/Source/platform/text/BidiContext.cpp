/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2009, 2010 Apple Inc.
 * All right reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "platform/text/BidiContext.h"

#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/Vector.h"

namespace blink {

using namespace WTF::Unicode;

struct SameSizeAsBidiContext
    : public ThreadSafeRefCounted<SameSizeAsBidiContext> {
  uint32_t bitfields : 16;
  void* parent;
};

static_assert(sizeof(BidiContext) == sizeof(SameSizeAsBidiContext),
              "BidiContext should stay small");

inline RefPtr<BidiContext> BidiContext::CreateUncached(
    unsigned char level,
    CharDirection direction,
    bool override,
    BidiEmbeddingSource source,
    BidiContext* parent) {
  return WTF::AdoptRef(
      new BidiContext(level, direction, override, source, parent));
}

RefPtr<BidiContext> BidiContext::Create(unsigned char level,
                                        CharDirection direction,
                                        bool override,
                                        BidiEmbeddingSource source,
                                        BidiContext* parent) {
  DCHECK_EQ(direction, (level % 2 ? kRightToLeft : kLeftToRight));

  if (parent || level >= 2)
    return CreateUncached(level, direction, override, source, parent);

  DCHECK_LE(level, 1);
  if (!level) {
    if (!override) {
      DEFINE_STATIC_REF(
          BidiContext, ltr_context,
          (CreateUncached(0, kLeftToRight, false, kFromStyleOrDOM, 0)));
      return ltr_context;
    }

    DEFINE_STATIC_REF(
        BidiContext, ltr_override_context,
        (CreateUncached(0, kLeftToRight, true, kFromStyleOrDOM, 0)));
    return ltr_override_context;
  }

  if (!override) {
    DEFINE_STATIC_REF(
        BidiContext, rtl_context,
        (CreateUncached(1, kRightToLeft, false, kFromStyleOrDOM, 0)));
    return rtl_context;
  }

  DEFINE_STATIC_REF(
      BidiContext, rtl_override_context,
      (CreateUncached(1, kRightToLeft, true, kFromStyleOrDOM, 0)));
  return rtl_override_context;
}

static inline RefPtr<BidiContext> CopyContextAndRebaselineLevel(
    BidiContext* context,
    BidiContext* parent) {
  DCHECK(context);
  unsigned char new_level = parent ? parent->Level() : 0;
  if (context->Dir() == kRightToLeft)
    new_level = NextGreaterOddLevel(new_level);
  else if (parent)
    new_level = NextGreaterEvenLevel(new_level);

  return BidiContext::Create(new_level, context->Dir(), context->Override(),
                             context->Source(), parent);
}

// The BidiContext stack must be immutable -- they're re-used for re-layout
// after DOM modification/editing -- so we copy all the non-unicode contexts,
// and recalculate their levels.
RefPtr<BidiContext> BidiContext::CopyStackRemovingUnicodeEmbeddingContexts() {
  Vector<BidiContext*, 64> contexts;
  for (BidiContext* iter = this; iter; iter = iter->Parent()) {
    if (iter->Source() != kFromUnicode)
      contexts.push_back(iter);
  }
  DCHECK(contexts.size());

  RefPtr<BidiContext> top_context =
      CopyContextAndRebaselineLevel(contexts.back(), 0);
  for (int i = contexts.size() - 1; i > 0; --i) {
    top_context =
        CopyContextAndRebaselineLevel(contexts[i - 1], top_context.get());
  }

  return top_context;
}

bool operator==(const BidiContext& c1, const BidiContext& c2) {
  if (&c1 == &c2)
    return true;
  if (c1.Level() != c2.Level() || c1.Override() != c2.Override() ||
      c1.Dir() != c2.Dir() || c1.Source() != c2.Source())
    return false;
  if (!c1.Parent())
    return !c2.Parent();
  return c2.Parent() && *c1.Parent() == *c2.Parent();
}

}  // namespace blink
