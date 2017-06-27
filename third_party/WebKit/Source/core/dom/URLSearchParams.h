// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URLSearchParams_h
#define URLSearchParams_h

#include <base/gtest_prod_util.h>
#include <utility>
#include "bindings/core/v8/Iterable.h"
#include "bindings/core/v8/USVStringSequenceSequenceOrUSVStringUSVStringRecordOrUSVString.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/network/EncodedFormData.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class DOMURL;

typedef USVStringSequenceSequenceOrUSVStringUSVStringRecordOrUSVString
    URLSearchParamsInit;

class CORE_EXPORT URLSearchParams final
    : public GarbageCollectedFinalized<URLSearchParams>,
      public ScriptWrappable,
      public PairIterable<String, String> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static URLSearchParams* Create(const URLSearchParamsInit&, ExceptionState&);
  static URLSearchParams* Create(const Vector<std::pair<String, String>>&,
                                 ExceptionState&);
  static URLSearchParams* Create(const Vector<Vector<String>>&,
                                 ExceptionState&);

  static URLSearchParams* Create(const String& query_string,
                                 DOMURL* url_object = nullptr) {
    return new URLSearchParams(query_string, url_object);
  }

  ~URLSearchParams();

  // URLSearchParams interface methods
  String toString() const;
  void append(const String& name, const String& value);
  void deleteAllWithName(const String&);
  String get(const String&) const;
  Vector<String> getAll(const String&) const;
  bool has(const String&) const;
  void set(const String& name, const String& value);
  void sort();
  void SetInput(const String&);

  // Internal helpers
  PassRefPtr<EncodedFormData> ToEncodedFormData() const;
  const Vector<std::pair<String, String>>& Params() const { return params_; }

#if DCHECK_IS_ON()
  DOMURL* UrlObject() const;
#endif

  DECLARE_TRACE();

 private:
  FRIEND_TEST_ALL_PREFIXES(URLSearchParamsTest, EncodedFormData);

  explicit URLSearchParams(const String&, DOMURL* = nullptr);

  void RunUpdateSteps();
  IterationSource* StartIteration(ScriptState*, ExceptionState&) override;
  void EncodeAsFormData(Vector<char>&) const;

  void AppendWithoutUpdate(const String& name, const String& value);

  Vector<std::pair<String, String>> params_;

  WeakMember<DOMURL> url_object_;
};

}  // namespace blink

#endif  // URLSearchParams_h
