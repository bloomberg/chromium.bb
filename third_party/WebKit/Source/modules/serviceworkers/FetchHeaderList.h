// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchHeaderList_h
#define FetchHeaderList_h

#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <utility>

namespace WebCore {

// http://fetch.spec.whatwg.org/#terminology-headers
class FetchHeaderList FINAL : public RefCounted<FetchHeaderList> {
public:
    typedef std::pair<String, String> Header;
    static PassRefPtr<FetchHeaderList> create();

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

    const Vector<OwnPtr<Header> >& list() const { return m_headerList; }

    static bool isValidHeaderName(const String&);
    static bool isValidHeaderValue(const String&);
    static bool isSimpleHeader(const String&, const String&);
    static bool isForbiddenHeaderName(const String&);
    static bool isForbiddenResponseHeaderName(const String&);

private:
    FetchHeaderList();
    Vector<OwnPtr<Header> > m_headerList;
};

} // namespace WebCore

#endif // FetchHeaderList_h
