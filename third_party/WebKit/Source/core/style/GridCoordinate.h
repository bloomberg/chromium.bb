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

#ifndef GridCoordinate_h
#define GridCoordinate_h

#include "core/style/GridPositionsResolver.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"
#include <algorithm>

namespace blink {

// Recommended maximum size for both explicit and implicit grids.
const int kGridMaxTracks = 1000000;

// A span in a single direction (either rows or columns). Note that |resolvedInitialPosition|
// and |resolvedFinalPosition| are grid lines' indexes.
// Iterating over the span shouldn't include |resolvedFinalPosition| to be correct.
struct GridSpan {
    USING_FAST_MALLOC(GridSpan);
public:

    static GridSpan untranslatedDefiniteGridSpan(int resolvedInitialPosition, int resolvedFinalPosition)
    {
        return GridSpan(resolvedInitialPosition, resolvedFinalPosition, UntranslatedDefinite);
    }

    static GridSpan translatedDefiniteGridSpan(size_t resolvedInitialPosition, size_t resolvedFinalPosition)
    {
        return GridSpan(resolvedInitialPosition, resolvedFinalPosition, TranslatedDefinite);
    }

    static GridSpan indefiniteGridSpan()
    {
        return GridSpan(0, 1, Indefinite);
    }

    bool operator==(const GridSpan& o) const
    {
        return m_type == o.m_type && m_resolvedInitialPosition == o.m_resolvedInitialPosition && m_resolvedFinalPosition == o.m_resolvedFinalPosition;
    }

    size_t integerSpan() const
    {
        ASSERT(isTranslatedDefinite());
        ASSERT(m_resolvedFinalPosition > m_resolvedInitialPosition);
        return m_resolvedFinalPosition - m_resolvedInitialPosition;
    }

    int untranslatedResolvedInitialPosition() const
    {
        ASSERT(m_type == UntranslatedDefinite);
        return m_resolvedInitialPosition;
    }

    int untranslatedResolvedFinalPosition() const
    {
        ASSERT(m_type == UntranslatedDefinite);
        return m_resolvedFinalPosition;
    }

    size_t resolvedInitialPosition() const
    {
        ASSERT(isTranslatedDefinite());
        ASSERT(m_resolvedInitialPosition >= 0);
        return m_resolvedInitialPosition;
    }

    size_t resolvedFinalPosition() const
    {
        ASSERT(isTranslatedDefinite());
        ASSERT(m_resolvedFinalPosition > 0);
        return m_resolvedFinalPosition;
    }

    struct GridSpanIterator {
        GridSpanIterator(size_t v) : value(v) {}

        size_t operator*() const { return value; }
        size_t operator++() { return value++; }
        bool operator!=(GridSpanIterator other) const { return value != other.value; }

        size_t value;
    };

    GridSpanIterator begin() const
    {
        ASSERT(isTranslatedDefinite());
        return m_resolvedInitialPosition;
    }

    GridSpanIterator end() const
    {
        ASSERT(isTranslatedDefinite());
        return m_resolvedFinalPosition;
    }

    bool isTranslatedDefinite() const
    {
        return m_type == TranslatedDefinite;
    }

    bool isIndefinite() const
    {
        return m_type == Indefinite;
    }

    void translate(size_t offset)
    {
        ASSERT(m_type == UntranslatedDefinite);

        m_type = TranslatedDefinite;
        m_resolvedInitialPosition += offset;
        m_resolvedFinalPosition += offset;

        ASSERT(m_resolvedInitialPosition >= 0);
        ASSERT(m_resolvedFinalPosition > 0);
    }

private:

    enum GridSpanType {UntranslatedDefinite, TranslatedDefinite, Indefinite};

    GridSpan(int resolvedInitialPosition, int resolvedFinalPosition, GridSpanType type)
        : m_type(type)
    {
#if ENABLE(ASSERT)
        ASSERT(resolvedInitialPosition < resolvedFinalPosition);
        if (type == TranslatedDefinite) {
            ASSERT(resolvedInitialPosition >= 0);
            ASSERT(resolvedFinalPosition > 0);
        }
#endif

        if (resolvedInitialPosition >= 0)
            m_resolvedInitialPosition = std::min(resolvedInitialPosition, kGridMaxTracks - 1);
        else
            m_resolvedInitialPosition = std::max(resolvedInitialPosition, -kGridMaxTracks);

        if (resolvedFinalPosition >= 0)
            m_resolvedFinalPosition = std::min(resolvedFinalPosition, kGridMaxTracks);
        else
            m_resolvedFinalPosition = std::max(resolvedFinalPosition, -kGridMaxTracks + 1);
    }

    int m_resolvedInitialPosition;
    int m_resolvedFinalPosition;
    GridSpanType m_type;
};

// This represents a grid area that spans in both rows' and columns' direction.
struct GridCoordinate {
    USING_FAST_MALLOC(GridCoordinate);
public:
    // HashMap requires a default constuctor.
    GridCoordinate()
        : columns(GridSpan::indefiniteGridSpan())
        , rows(GridSpan::indefiniteGridSpan())
    {
    }

    GridCoordinate(const GridSpan& r, const GridSpan& c)
        : columns(c)
        , rows(r)
    {
    }

    bool operator==(const GridCoordinate& o) const
    {
        return columns == o.columns && rows == o.rows;
    }

    bool operator!=(const GridCoordinate& o) const
    {
        return !(*this == o);
    }

    GridSpan columns;
    GridSpan rows;
};

typedef HashMap<String, GridCoordinate> NamedGridAreaMap;

} // namespace blink

#endif // GridCoordinate_h
