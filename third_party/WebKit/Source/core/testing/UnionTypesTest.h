// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UnionTypesTest_h
#define UnionTypesTest_h

#include "bindings/core/v8/DoubleOrInternalEnum.h"
#include "bindings/core/v8/DoubleOrString.h"
#include "bindings/core/v8/DoubleOrStringOrStringSequence.h"
#include "bindings/core/v8/NodeListOrElement.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class UnionTypesTest final : public GarbageCollectedFinalized<UnionTypesTest>,
                             public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static UnionTypesTest* Create() { return new UnionTypesTest(); }
  virtual ~UnionTypesTest() {}

  void doubleOrStringOrStringSequenceAttribute(DoubleOrStringOrStringSequence&);
  void setDoubleOrStringOrStringSequenceAttribute(
      const DoubleOrStringOrStringSequence&);

  String doubleOrStringArg(DoubleOrString&);
  String doubleOrInternalEnumArg(DoubleOrInternalEnum&);
  String doubleOrStringSequenceArg(HeapVector<DoubleOrString>&);

  String nodeListOrElementArg(NodeListOrElement&);
  String nodeListOrElementOrNullArg(NodeListOrElement&);

  String doubleOrStringOrStringSequenceArg(
      const DoubleOrStringOrStringSequence&);

  DEFINE_INLINE_TRACE() {}

 private:
  UnionTypesTest() : attribute_type_(kSpecificTypeNone) {}

  enum AttributeSpecificType {
    kSpecificTypeNone,
    kSpecificTypeDouble,
    kSpecificTypeString,
    kSpecificTypeStringSequence,
  };
  AttributeSpecificType attribute_type_;
  double attribute_double_;
  String attribute_string_;
  Vector<String> attribute_string_sequence_;
};

}  // namespace blink

#endif  // UnionTypesTest_h
