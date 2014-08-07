// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchHeaderList_h
#define FetchHeaderList_h

#include "platform/heap/Handle.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <utility>

namespace blink {

class Header;

// http://fetch.spec.whatwg.org/#terminology-headers
class FetchHeaderList FINAL : public RefCountedWillBeGarbageCollectedFinalized<FetchHeaderList> {
public:
    typedef std::pair<String, String> Header;
    static PassRefPtrWillBeRawPtr<FetchHeaderList> create();
    PassRefPtrWillBeRawPtr<FetchHeaderList> createCopy();

    ~FetchHeaderList();
    void append(const String&, const String&);
    void set(const String&, const String&);
    // FIXME: Implement parse()
    // FIXME: Implement extractMIMEType()

    size_t size() const;
    void remove(const String&);
    bool get(const String&, String&) const;
    void getAll(const String&, Vector<String>&) const;
    bool has(const String&) const;
    void clearList();

    bool containsNonSimpleHeader() const;

    const Vector<OwnPtr<Header> >& list() const { return m_headerList; }

    static bool isValidHeaderName(const String&);
    static bool isValidHeaderValue(const String&);

    void trace(Visitor*) { }

private:
    FetchHeaderList();
    Vector<OwnPtr<Header> > m_headerList;
};

} // namespace blink

#endif // FetchHeaderList_h
