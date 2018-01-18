// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/AtRuleDescriptorSerializer.h"

#include "platform/wtf/text/StringBuilder.h"

namespace blink {

namespace {

void AppendNameValuePair(AtRuleDescriptorID id,
                         const String& value,
                         StringBuilder& builder) {
  if (!builder.IsEmpty())
    builder.Append(' ');
  builder.Append(GetDescriptorName(id));
  builder.Append(": ");
  builder.Append(value);
  builder.Append(';');
}

void AppendIfNotEmpty(AtRuleDescriptorID id,
                      const AtRuleDescriptorValueSet& descriptor_value_set,
                      StringBuilder& builder) {
  const CSSValue* value = descriptor_value_set.GetPropertyCSSValue(id);
  if (value)
    AppendNameValuePair(id, value->CssText(), builder);
}

String SerializeFontFace(const AtRuleDescriptorValueSet& descriptor_value_set) {
  StringBuilder builder;

  // The FontFace spec requires that the values are serialized in a specific
  // order. See: https://drafts.csswg.org/cssom/#serialize-a-css-rule
  const CSSValue* font_family_value =
      descriptor_value_set.GetPropertyCSSValue(AtRuleDescriptorID::FontFamily);
  // Font family is non-optional.
  if (!font_family_value)
    return "";
  AppendNameValuePair(AtRuleDescriptorID::FontFamily,
                      font_family_value->CssText(), builder);

  AppendIfNotEmpty(AtRuleDescriptorID::Src, descriptor_value_set, builder);
  AppendIfNotEmpty(AtRuleDescriptorID::UnicodeRange, descriptor_value_set,
                   builder);
  AppendIfNotEmpty(AtRuleDescriptorID::FontVariant, descriptor_value_set,
                   builder);
  AppendIfNotEmpty(AtRuleDescriptorID::FontFeatureSettings,
                   descriptor_value_set, builder);
  AppendIfNotEmpty(AtRuleDescriptorID::FontStretch, descriptor_value_set,
                   builder);
  AppendIfNotEmpty(AtRuleDescriptorID::FontWeight, descriptor_value_set,
                   builder);
  AppendIfNotEmpty(AtRuleDescriptorID::FontStyle, descriptor_value_set,
                   builder);

  // The order of font-display is not specified in the above link.
  AppendIfNotEmpty(AtRuleDescriptorID::FontDisplay, descriptor_value_set,
                   builder);

  return builder.ToString();
}

}  // namespace

String AtRuleDescriptorSerializer::SerializeAtRuleDescriptors(
    const AtRuleDescriptorValueSet& descriptor_value_set) {
  if (descriptor_value_set.GetType() ==
      AtRuleDescriptorValueSet::kFontFaceType) {
    return SerializeFontFace(descriptor_value_set);
  }
  NOTREACHED();
  return "";
}

}  // namespace blink
