// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FetchHeaderList.h"

#include "platform/loader/fetch/FetchUtils.h"
#include "platform/network/HTTPParsers.h"
#include "wtf/PtrUtil.h"
#include <algorithm>

namespace blink {

FetchHeaderList* FetchHeaderList::Create() {
  return new FetchHeaderList();
}

FetchHeaderList* FetchHeaderList::Clone() const {
  FetchHeaderList* list = Create();
  for (size_t i = 0; i < header_list_.size(); ++i)
    list->Append(header_list_[i]->first, header_list_[i]->second);
  return list;
}

FetchHeaderList::FetchHeaderList() {}

FetchHeaderList::~FetchHeaderList() {}

void FetchHeaderList::Append(const String& name, const String& value) {
  // "To append a name/value (|name|/|value|) pair to a header list (|list|),
  // append a new header whose name is |name|, byte lowercased, and value is
  // |value|, to |list|."
  header_list_.push_back(WTF::WrapUnique(new Header(name.Lower(), value)));
}

void FetchHeaderList::Set(const String& name, const String& value) {
  // "To set a name/value (|name|/|value|) pair in a header list (|list|), run
  // these steps:
  // 1. Byte lowercase |name|.
  // 2. If there are any headers in |list| whose name is |name|, set the value
  //    of the first such header to |value| and remove the others.
  // 3. Otherwise, append a new header whose name is |name| and value is
  //    |value|, to |list|."
  const String lowercased_name = name.Lower();
  for (size_t i = 0; i < header_list_.size(); ++i) {
    if (header_list_[i]->first == lowercased_name) {
      header_list_[i]->second = value;
      for (size_t j = i + 1; j < header_list_.size();) {
        if (header_list_[j]->first == lowercased_name)
          header_list_.erase(j);
        else
          ++j;
      }
      return;
    }
  }
  header_list_.push_back(WTF::MakeUnique<Header>(lowercased_name, value));
}

String FetchHeaderList::ExtractMIMEType() const {
  // To extract a MIME type from a header list (headers), run these steps:
  // 1. Let MIMEType be the result of parsing `Content-Type` in headers.
  String mime_type;
  if (!Get("Content-Type", mime_type)) {
    // 2. If MIMEType is null or failure, return the empty byte sequence.
    return String();
  }
  // 3. Return MIMEType, byte lowercased.
  return mime_type.Lower();
}

size_t FetchHeaderList::size() const {
  return header_list_.size();
}

void FetchHeaderList::Remove(const String& name) {
  // "To delete a name (|name|) from a header list (|list|), remove all headers
  // whose name is |name|, byte lowercased, from |list|."
  const String lowercased_name = name.Lower();
  for (size_t i = 0; i < header_list_.size();) {
    if (header_list_[i]->first == lowercased_name)
      header_list_.erase(i);
    else
      ++i;
  }
}

bool FetchHeaderList::Get(const String& name, String& result) const {
  const String lowercased_name = name.Lower();
  bool found = false;
  for (const auto& header : header_list_) {
    if (header->first == lowercased_name) {
      if (!found) {
        result = "";
        result.Append(header->second);
        found = true;
      } else {
        result.Append(",");
        result.Append(header->second);
      }
    }
  }
  return found;
}

void FetchHeaderList::GetAll(const String& name, Vector<String>& result) const {
  const String lowercased_name = name.Lower();
  result.Clear();
  for (size_t i = 0; i < header_list_.size(); ++i) {
    if (header_list_[i]->first == lowercased_name)
      result.push_back(header_list_[i]->second);
  }
}

bool FetchHeaderList::Has(const String& name) const {
  const String lowercased_name = name.Lower();
  for (size_t i = 0; i < header_list_.size(); ++i) {
    if (header_list_[i]->first == lowercased_name)
      return true;
  }
  return false;
}

void FetchHeaderList::ClearList() {
  header_list_.Clear();
}

bool FetchHeaderList::ContainsNonSimpleHeader() const {
  for (size_t i = 0; i < header_list_.size(); ++i) {
    if (!FetchUtils::IsSimpleHeader(AtomicString(header_list_[i]->first),
                                    AtomicString(header_list_[i]->second)))
      return true;
  }
  return false;
}

void FetchHeaderList::SortAndCombine() {
  // https://fetch.spec.whatwg.org/#concept-header-list-sort-and-combine
  // "To sort and combine a header list..."
  if (header_list_.IsEmpty())
    return;

  std::sort(
      header_list_.begin(), header_list_.end(),
      [](const std::unique_ptr<Header>& a, const std::unique_ptr<Header>& b) {
        return WTF::CodePointCompareLessThan(a->first, b->first);
      });

  for (size_t index = header_list_.size() - 1; index > 0; --index) {
    if (header_list_[index - 1]->first == header_list_[index]->first) {
      header_list_[index - 1]->second.Append(",");
      header_list_[index - 1]->second.Append(header_list_[index]->second);
      header_list_.erase(index, 1);
    }
  }
}

bool FetchHeaderList::IsValidHeaderName(const String& name) {
  // "A name is a case-insensitive byte sequence that matches the field-name
  // token production."
  return IsValidHTTPToken(name);
}

bool FetchHeaderList::IsValidHeaderValue(const String& value) {
  // "A value is a byte sequence that matches the field-value token production
  // and contains no 0x0A or 0x0D bytes."
  return IsValidHTTPHeaderValue(value);
}

}  // namespace blink
