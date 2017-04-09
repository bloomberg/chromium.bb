// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchHeaderList_h
#define FetchHeaderList_h

#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <memory>
#include <utility>

namespace blink {

class Header;

// http://fetch.spec.whatwg.org/#terminology-headers
class MODULES_EXPORT FetchHeaderList final
    : public GarbageCollectedFinalized<FetchHeaderList> {
 public:
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

  bool ContainsNonSimpleHeader() const;
  void SortAndCombine();

  const Vector<std::unique_ptr<Header>>& List() const { return header_list_; }
  const Header& Entry(size_t index) const {
    return *(header_list_[index].get());
  }

  static bool IsValidHeaderName(const String&);
  static bool IsValidHeaderValue(const String&);

  DEFINE_INLINE_TRACE() {}

 private:
  FetchHeaderList();
  Vector<std::unique_ptr<Header>> header_list_;
};

}  // namespace blink

#endif  // FetchHeaderList_h
