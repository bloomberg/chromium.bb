// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SubresourceIntegrity_h
#define SubresourceIntegrity_h

#include "platform/Crypto.h"

namespace WTF {
class String;
};

namespace blink {

class Document;
class Element;
class KURL;

class SubresourceIntegrity {
public:
    static bool CheckSubresourceIntegrity(const Element&, const WTF::String&, const KURL& resourceUrl);

private:
    // FIXME: After the merge with the Chromium repo, this should be refactored
    // to use FRIEND_TEST in base/gtest_prod_util.h.
    friend class SubresourceIntegrityTest;

    static bool parseAlgorithm(const UChar*& begin, const UChar* end, HashAlgorithm&);
    static bool parseDigest(const UChar*& begin, const UChar* end, String& digest);

    static bool parseIntegrityAttribute(const WTF::String& attribute, WTF::String& integrity, HashAlgorithm&, Document&);
};

} // namespace blink

#endif
