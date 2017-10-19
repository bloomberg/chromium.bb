// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchHeaderList_h
#define FetchHeaderList_h

#include <map>
#include <utility>
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

// http://fetch.spec.whatwg.org/#terminology-headers
class MODULES_EXPORT FetchHeaderList final
    : public GarbageCollectedFinalized<FetchHeaderList> {
 public:
  struct ByteCaseInsensitiveCompare {
    bool operator()(const String& lhs, const String& rhs) const {
      return CodePointCompareLessThan(lhs.LowerASCII(), rhs.LowerASCII());
    }
  };

  typedef std::pair<String, String> Header;
  static FetchHeaderList* Create();
  FetchHeaderList* Clone() const;

  ~FetchHeaderList();
  void Append(const String&, const String&);
  void Set(const String&, const String&);
  // FIXME: Implement parse()
  String ExtractMIMEType() const;

  size_t size() const;
  void Remove(const String&);
  bool Get(const String&, String&) const;
  void GetAll(const String&, Vector<String>&) const;
  bool Has(const String&) const;
  void ClearList();

  bool ContainsNonCORSSafelistedHeader() const;
  Vector<Header> SortAndCombine() const;

  const std::multimap<String, String, ByteCaseInsensitiveCompare>& List()
      const {
    return header_list_;
  }

  static bool IsValidHeaderName(const String&);
  static bool IsValidHeaderValue(const String&);

  void Trace(blink::Visitor* visitor) {}

 private:
  FetchHeaderList();

  // While using STL data structures in Blink is not very common or
  // encouraged, we do need a multimap here. The closest WTF structure
  // comparable to what we need would be a
  //   HashMap<String, Vector<String>>
  // but it is not a "flat" data structure like std::multimap is. The
  // size() of the HashMap is the number of distinct header names, not
  // the total number of headers and values on the list.
  // This would cause FetchHeaderList::size() to have to manually
  // iterate through all keys and vectors in the HashMap. Similarly,
  // list() would require callers to manually iterate through the
  // HashMap's keys and value vector, and so would
  // ContainsNonCORSSafelistedHeader().
  std::multimap<String, String, ByteCaseInsensitiveCompare> header_list_;
};

}  // namespace blink

#endif  // FetchHeaderList_h
