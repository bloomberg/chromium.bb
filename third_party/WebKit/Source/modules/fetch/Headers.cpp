// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/Headers.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8IteratorResultValue.h"
#include "bindings/modules/v8/ByteStringSequenceSequenceOrByteStringByteStringRecordOrHeaders.h"
#include "core/dom/Iterator.h"
#include "platform/loader/fetch/FetchUtils.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

namespace {

class HeadersIterationSource final
    : public PairIterable<String, String>::IterationSource {
 public:
  explicit HeadersIterationSource(const FetchHeaderList* headers)
      : headers_(headers->SortAndCombine()), current_(0) {}

  bool Next(ScriptState* script_state,
            String& key,
            String& value,
            ExceptionState& exception) override {
    // This simply advances an index and returns the next value if any; the
    // iterated list is not exposed to script so it will never be mutated
    // during iteration.
    if (current_ >= headers_.size())
      return false;

    const FetchHeaderList::Header& header = headers_.at(current_++);
    key = header.first;
    value = header.second;
    return true;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    PairIterable<String, String>::IterationSource::Trace(visitor);
  }

 private:
  Vector<std::pair<String, String>> headers_;
  size_t current_;
};

}  // namespace

Headers* Headers::Create(ExceptionState&) {
  return new Headers;
}

Headers* Headers::Create(
    const ByteStringSequenceSequenceOrByteStringByteStringRecordOrHeaders& init,
    ExceptionState& exception_state) {
  // "The Headers(|init|) constructor, when invoked, must run these steps:"
  // "1. Let |headers| be a new Headers object whose guard is "none".
  Headers* headers = Create(exception_state);
  // "2. If |init| is given, fill headers with |init|. Rethrow any exception."
  if (init.isByteStringSequenceSequence()) {
    headers->FillWith(init.getAsByteStringSequenceSequence(), exception_state);
  } else if (init.isByteStringByteStringRecord()) {
    headers->FillWith(init.getAsByteStringByteStringRecord(), exception_state);
  } else if (init.isHeaders()) {
    // This branch will not be necessary once http://crbug.com/690428 is fixed.
    headers->FillWith(init.getAsHeaders(), exception_state);
  } else {
    NOTREACHED();
  }
  // "3. Return |headers|."
  return headers;
}

Headers* Headers::Create(FetchHeaderList* header_list) {
  return new Headers(header_list);
}

Headers* Headers::Clone() const {
  FetchHeaderList* header_list = header_list_->Clone();
  Headers* headers = Create(header_list);
  headers->guard_ = guard_;
  return headers;
}

void Headers::append(const String& name,
                     const String& value,
                     ExceptionState& exception_state) {
  // "To append a name/value (|name|/|value|) pair to a Headers object
  // (|headers|), run these steps:"
  // "1. If |name| is not a name or |value| is not a value, throw a
  //     TypeError."
  if (!FetchHeaderList::IsValidHeaderName(name)) {
    exception_state.ThrowTypeError("Invalid name");
    return;
  }
  if (!FetchHeaderList::IsValidHeaderValue(value)) {
    exception_state.ThrowTypeError("Invalid value");
    return;
  }
  // "2. If guard is |request|, throw a TypeError."
  if (guard_ == kImmutableGuard) {
    exception_state.ThrowTypeError("Headers are immutable");
    return;
  }
  // "3. Otherwise, if guard is |request| and |name| is a forbidden header
  //     name, return."
  if (guard_ == kRequestGuard && FetchUtils::IsForbiddenHeaderName(name))
    return;
  // "4. Otherwise, if guard is |request-no-CORS| and |name|/|value| is not a
  //     simple header, return."
  if (guard_ == kRequestNoCORSGuard &&
      !FetchUtils::IsSimpleHeader(AtomicString(name), AtomicString(value)))
    return;
  // "5. Otherwise, if guard is |response| and |name| is a forbidden response
  //     header name, return."
  if (guard_ == kResponseGuard &&
      FetchUtils::IsForbiddenResponseHeaderName(name))
    return;
  // "6. Append |name|/|value| to header list."
  header_list_->Append(name, value);
}

void Headers::remove(const String& name, ExceptionState& exception_state) {
  // "The delete(|name|) method, when invoked, must run these steps:"
  // "1. If name is not a name, throw a TypeError."
  if (!FetchHeaderList::IsValidHeaderName(name)) {
    exception_state.ThrowTypeError("Invalid name");
    return;
  }
  // "2. If guard is |immutable|, throw a TypeError."
  if (guard_ == kImmutableGuard) {
    exception_state.ThrowTypeError("Headers are immutable");
    return;
  }
  // "3. Otherwise, if guard is |request| and |name| is a forbidden header
  //     name, return."
  if (guard_ == kRequestGuard && FetchUtils::IsForbiddenHeaderName(name))
    return;
  // "4. Otherwise, if guard is |request-no-CORS| and |name|/`invalid` is not
  //     a simple header, return."
  if (guard_ == kRequestNoCORSGuard &&
      !FetchUtils::IsSimpleHeader(AtomicString(name), "invalid"))
    return;
  // "5. Otherwise, if guard is |response| and |name| is a forbidden response
  //     header name, return."
  if (guard_ == kResponseGuard &&
      FetchUtils::IsForbiddenResponseHeaderName(name))
    return;
  // "6. Delete |name| from header list."
  header_list_->Remove(name);
}

String Headers::get(const String& name, ExceptionState& exception_state) {
  // "The get(|name|) method, when invoked, must run these steps:"
  // "1. If |name| is not a name, throw a TypeError."
  if (!FetchHeaderList::IsValidHeaderName(name)) {
    exception_state.ThrowTypeError("Invalid name");
    return String();
  }
  // "2. If there is no header in header list whose name is |name|,
  //     return null."
  // "3. Return the combined value given |name| and header list."
  String result;
  header_list_->Get(name, result);
  return result;
}

Vector<String> Headers::getAll(const String& name,
                               ExceptionState& exception_state) {
  // "The getAll(|name|) method, when invoked, must run these steps:"
  // "1. If |name| is not a name, throw a TypeError."
  if (!FetchHeaderList::IsValidHeaderName(name)) {
    exception_state.ThrowTypeError("Invalid name");
    return Vector<String>();
  }
  // "2. Return the values of all headers in header list whose name is |name|,
  //     in list order, and the empty sequence otherwise."
  Vector<String> result;
  header_list_->GetAll(name, result);
  return result;
}

bool Headers::has(const String& name, ExceptionState& exception_state) {
  // "The has(|name|) method, when invoked, must run these steps:"
  // "1. If |name| is not a name, throw a TypeError."
  if (!FetchHeaderList::IsValidHeaderName(name)) {
    exception_state.ThrowTypeError("Invalid name");
    return false;
  }
  // "2. Return true if there is a header in header list whose name is |name|,
  //     and false otherwise."
  return header_list_->Has(name);
}

void Headers::set(const String& name,
                  const String& value,
                  ExceptionState& exception_state) {
  // "The set(|name|, |value|) method, when invoked, must run these steps:"
  // "1. If |name| is not a name or |value| is not a value, throw a
  //     TypeError."
  if (!FetchHeaderList::IsValidHeaderName(name)) {
    exception_state.ThrowTypeError("Invalid name");
    return;
  }
  if (!FetchHeaderList::IsValidHeaderValue(value)) {
    exception_state.ThrowTypeError("Invalid value");
    return;
  }
  // "2. If guard is |immutable|, throw a TypeError."
  if (guard_ == kImmutableGuard) {
    exception_state.ThrowTypeError("Headers are immutable");
    return;
  }
  // "3. Otherwise, if guard is |request| and |name| is a forbidden header
  //     name, return."
  if (guard_ == kRequestGuard && FetchUtils::IsForbiddenHeaderName(name))
    return;
  // "4. Otherwise, if guard is |request-no-CORS| and |name|/|value| is not a
  //     simple header, return."
  if (guard_ == kRequestNoCORSGuard &&
      !FetchUtils::IsSimpleHeader(AtomicString(name), AtomicString(value)))
    return;
  // "5. Otherwise, if guard is |response| and |name| is a forbidden response
  //     header name, return."
  if (guard_ == kResponseGuard &&
      FetchUtils::IsForbiddenResponseHeaderName(name))
    return;
  // "6. Set |name|/|value| in header list."
  header_list_->Set(name, value);
}

void Headers::FillWith(const Headers* object, ExceptionState& exception_state) {
  DCHECK(header_list_->size() == 0);
  // There used to be specific steps describing filling a Headers object with
  // another Headers object, but it has since been removed because it should be
  // handled like a sequence (http://crbug.com/690428).
  for (const auto& header : object->header_list_->List()) {
    append(header.first, header.second, exception_state);
    if (exception_state.HadException())
      return;
  }
}

void Headers::FillWith(const Vector<Vector<String>>& object,
                       ExceptionState& exception_state) {
  DCHECK(!header_list_->size());
  // "1. If |object| is a sequence, then for each |header| in |object|, run
  //     these substeps:
  //     1. If |header| does not contain exactly two items, then throw a
  //        TypeError.
  //     2. Append |header|’s first item/|header|’s second item to |headers|.
  //        Rethrow any exception."
  for (size_t i = 0; i < object.size(); ++i) {
    if (object[i].size() != 2) {
      exception_state.ThrowTypeError("Invalid value");
      return;
    }
    append(object[i][0], object[i][1], exception_state);
    if (exception_state.HadException())
      return;
  }
}

void Headers::FillWith(const Vector<std::pair<String, String>>& object,
                       ExceptionState& exception_state) {
  DCHECK(!header_list_->size());

  for (const auto& item : object) {
    append(item.first, item.second, exception_state);
    if (exception_state.HadException())
      return;
  }
}

Headers::Headers()
    : header_list_(FetchHeaderList::Create()), guard_(kNoneGuard) {}

Headers::Headers(FetchHeaderList* header_list)
    : header_list_(header_list), guard_(kNoneGuard) {}

DEFINE_TRACE(Headers) {
  visitor->Trace(header_list_);
}

PairIterable<String, String>::IterationSource* Headers::StartIteration(
    ScriptState*,
    ExceptionState&) {
  return new HeadersIterationSource(header_list_);
}

}  // namespace blink
