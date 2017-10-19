// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UnionTypesTest_h
#define UnionTypesTest_h

#include "bindings/core/v8/double_or_internal_enum.h"
#include "bindings/core/v8/double_or_string.h"
#include "bindings/core/v8/double_or_string_or_string_sequence.h"
#include "bindings/core/v8/node_list_or_element.h"
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

  void Trace(blink::Visitor* visitor) {}

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
