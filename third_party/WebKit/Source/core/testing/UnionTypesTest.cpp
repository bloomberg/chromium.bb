// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/UnionTypesTest.h"

#include "platform/wtf/text/StringBuilder.h"

namespace blink {

void UnionTypesTest::doubleOrStringOrStringSequenceAttribute(
    DoubleOrStringOrStringSequence& double_or_string_or_string_sequence) {
  switch (attribute_type_) {
    case kSpecificTypeNone:
      // Default value is zero (of double).
      double_or_string_or_string_sequence.setDouble(0);
      break;
    case kSpecificTypeDouble:
      double_or_string_or_string_sequence.setDouble(attribute_double_);
      break;
    case kSpecificTypeString:
      double_or_string_or_string_sequence.setString(attribute_string_);
      break;
    case kSpecificTypeStringSequence:
      double_or_string_or_string_sequence.setStringSequence(
          attribute_string_sequence_);
      break;
    default:
      NOTREACHED();
  }
}

void UnionTypesTest::setDoubleOrStringOrStringSequenceAttribute(
    const DoubleOrStringOrStringSequence& double_or_string_or_string_sequence) {
  if (double_or_string_or_string_sequence.isDouble()) {
    attribute_double_ = double_or_string_or_string_sequence.getAsDouble();
    attribute_type_ = kSpecificTypeDouble;
  } else if (double_or_string_or_string_sequence.isString()) {
    attribute_string_ = double_or_string_or_string_sequence.getAsString();
    attribute_type_ = kSpecificTypeString;
  } else if (double_or_string_or_string_sequence.isStringSequence()) {
    attribute_string_sequence_ =
        double_or_string_or_string_sequence.getAsStringSequence();
    attribute_type_ = kSpecificTypeStringSequence;
  } else {
    NOTREACHED();
  }
}

String UnionTypesTest::doubleOrStringArg(DoubleOrString& double_or_string) {
  if (double_or_string.isNull())
    return "null is passed";
  if (double_or_string.isDouble())
    return "double is passed: " +
           String::NumberToStringECMAScript(double_or_string.getAsDouble());
  if (double_or_string.isString())
    return "string is passed: " + double_or_string.getAsString();
  NOTREACHED();
  return String();
}

String UnionTypesTest::doubleOrInternalEnumArg(
    DoubleOrInternalEnum& double_or_internal_enum) {
  if (double_or_internal_enum.isDouble())
    return "double is passed: " + String::NumberToStringECMAScript(
                                      double_or_internal_enum.getAsDouble());
  if (double_or_internal_enum.isInternalEnum())
    return "InternalEnum is passed: " +
           double_or_internal_enum.getAsInternalEnum();
  NOTREACHED();
  return String();
}

String UnionTypesTest::doubleOrStringSequenceArg(
    HeapVector<DoubleOrString>& sequence) {
  if (!sequence.size())
    return "";

  StringBuilder builder;
  for (DoubleOrString& double_or_string : sequence) {
    DCHECK(!double_or_string.isNull());
    if (double_or_string.isDouble()) {
      builder.Append("double: ");
      builder.Append(
          String::NumberToStringECMAScript(double_or_string.getAsDouble()));
    } else if (double_or_string.isString()) {
      builder.Append("string: ");
      builder.Append(double_or_string.getAsString());
    } else {
      NOTREACHED();
    }
    builder.Append(", ");
  }
  return builder.Substring(0, builder.length() - 2);
}

String UnionTypesTest::nodeListOrElementArg(
    NodeListOrElement& node_list_or_element) {
  DCHECK(!node_list_or_element.isNull());
  return nodeListOrElementOrNullArg(node_list_or_element);
}

String UnionTypesTest::nodeListOrElementOrNullArg(
    NodeListOrElement& node_list_or_element_or_null) {
  if (node_list_or_element_or_null.isNull())
    return "null or undefined is passed";
  if (node_list_or_element_or_null.isNodeList())
    return "nodelist is passed";
  if (node_list_or_element_or_null.isElement())
    return "element is passed";
  NOTREACHED();
  return String();
}

String UnionTypesTest::doubleOrStringOrStringSequenceArg(
    const DoubleOrStringOrStringSequence& double_or_string_or_string_sequence) {
  if (double_or_string_or_string_sequence.isNull())
    return "null";

  if (double_or_string_or_string_sequence.isDouble())
    return "double: " + String::NumberToStringECMAScript(
                            double_or_string_or_string_sequence.getAsDouble());

  if (double_or_string_or_string_sequence.isString())
    return "string: " + double_or_string_or_string_sequence.getAsString();

  DCHECK(double_or_string_or_string_sequence.isStringSequence());
  const Vector<String>& sequence =
      double_or_string_or_string_sequence.getAsStringSequence();
  if (!sequence.size())
    return "sequence: []";
  StringBuilder builder;
  builder.Append("sequence: [");
  for (const String& item : sequence) {
    DCHECK(!item.IsNull());
    builder.Append(item);
    builder.Append(", ");
  }
  return builder.Substring(0, builder.length() - 2) + "]";
}

}  // namespace blink
