// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/UnionTypesTest.h"

#include "platform/wtf/text/StringBuilder.h"

namespace blink {

void UnionTypesTest::doubleOrStringOrStringArrayAttribute(
    DoubleOrStringOrStringArray& double_or_string_or_string_array) {
  switch (attribute_type_) {
    case kSpecificTypeNone:
      // Default value is zero (of double).
      double_or_string_or_string_array.setDouble(0);
      break;
    case kSpecificTypeDouble:
      double_or_string_or_string_array.setDouble(attribute_double_);
      break;
    case kSpecificTypeString:
      double_or_string_or_string_array.setString(attribute_string_);
      break;
    case kSpecificTypeStringArray:
      double_or_string_or_string_array.setStringArray(attribute_string_array_);
      break;
    default:
      NOTREACHED();
  }
}

void UnionTypesTest::setDoubleOrStringOrStringArrayAttribute(
    const DoubleOrStringOrStringArray& double_or_string_or_string_array) {
  if (double_or_string_or_string_array.isDouble()) {
    attribute_double_ = double_or_string_or_string_array.getAsDouble();
    attribute_type_ = kSpecificTypeDouble;
  } else if (double_or_string_or_string_array.isString()) {
    attribute_string_ = double_or_string_or_string_array.getAsString();
    attribute_type_ = kSpecificTypeString;
  } else if (double_or_string_or_string_array.isStringArray()) {
    attribute_string_array_ =
        double_or_string_or_string_array.getAsStringArray();
    attribute_type_ = kSpecificTypeStringArray;
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

String UnionTypesTest::doubleOrStringArrayArg(
    HeapVector<DoubleOrString>& array) {
  if (!array.size())
    return "";

  StringBuilder builder;
  for (DoubleOrString& double_or_string : array) {
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

String UnionTypesTest::doubleOrStringSequenceArg(
    HeapVector<DoubleOrString>& sequence) {
  return doubleOrStringArrayArg(sequence);
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

String UnionTypesTest::doubleOrStringOrStringArrayArg(
    const DoubleOrStringOrStringArray& double_or_string_or_string_array) {
  if (double_or_string_or_string_array.isNull())
    return "null";

  if (double_or_string_or_string_array.isDouble())
    return "double: " + String::NumberToStringECMAScript(
                            double_or_string_or_string_array.getAsDouble());

  if (double_or_string_or_string_array.isString())
    return "string: " + double_or_string_or_string_array.getAsString();

  DCHECK(double_or_string_or_string_array.isStringArray());
  const Vector<String>& array =
      double_or_string_or_string_array.getAsStringArray();
  if (!array.size())
    return "array: []";
  StringBuilder builder;
  builder.Append("array: [");
  for (const String& item : array) {
    DCHECK(!item.IsNull());
    builder.Append(item);
    builder.Append(", ");
  }
  return builder.Substring(0, builder.length() - 2) + "]";
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
