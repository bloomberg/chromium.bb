// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/form_field.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebLabelElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNodeList.h"

using WebKit::WebElement;
using WebKit::WebLabelElement;
using WebKit::WebInputElement;
using WebKit::WebNode;
using WebKit::WebNodeList;

// TODO(jhawkins): Remove the following methods once AutoFill has been switched
// over to using FormData.
// WARNING: This code must stay in sync with the corresponding code in
// FormManager until we can remove this.
namespace {

string16 InferLabelForElement(const WebInputElement& element) {
  string16 inferred_label;
  WebNode previous = element.previousSibling();
  if (!previous.isNull()) {
    if (previous.isTextNode()) {
      inferred_label = previous.nodeValue();
      TrimWhitespace(inferred_label, TRIM_ALL, &inferred_label);
    }

    // If we didn't find text, check for previous paragraph.
    // Eg. <p>Some Text</p><input ...>
    // Note the lack of whitespace between <p> and <input> elements.
    if (inferred_label.empty()) {
      if (previous.isElementNode()) {
        WebElement element = previous.toElement<WebElement>();
        if (element.hasTagName("p")) {
          inferred_label = element.innerText();
          TrimWhitespace(inferred_label, TRIM_ALL, &inferred_label);
        }
      }
    }

    // If we didn't find paragraph, check for previous paragraph to this.
    // Eg. <p>Some Text</p>   <input ...>
    // Note the whitespace between <p> and <input> elements.
    if (inferred_label.empty()) {
      previous = previous.previousSibling();
      if (!previous.isNull() && previous.isElementNode()) {
        WebElement element = previous.toElement<WebElement>();
        if (element.hasTagName("p")) {
          inferred_label = element.innerText();
          TrimWhitespace(inferred_label, TRIM_ALL, &inferred_label);
        }
      }
    }
  }

  return inferred_label;
}

string16 LabelForElement(const WebInputElement& element) {
  WebNodeList labels = element.document().getElementsByTagName("label");
  for (unsigned i = 0; i < labels.length(); ++i) {
    WebElement e = labels.item(i).toElement<WebElement>();
    if (e.hasTagName("label")) {
      WebLabelElement label = e.toElement<WebLabelElement>();
      if (label.correspondingControl() == element)
        return label.innerText();
    }
  }

  // Infer the label from context if not found in label element.
  return InferLabelForElement(element);
}

}  // namespace

namespace webkit_glue {

FormField::FormField() {
}

FormField::FormField(const WebInputElement& input_element) {
  name_ = input_element.nameForAutofill();
  label_ = LabelForElement(input_element);

  value_ = input_element.value();
  TrimWhitespace(value_, TRIM_LEADING, &value_);

  form_control_type_ = input_element.formControlType();
  input_type_ = input_element.inputType();
}

FormField::FormField(const string16& label,
                     const string16& name,
                     const string16& value,
                     const string16& form_control_type,
                     WebInputElement::InputType input_type)
  : label_(label),
    name_(name),
    value_(value),
    form_control_type_(form_control_type),
    input_type_(input_type) {
}

bool FormField::operator==(const FormField& field) const {
  // A FormField stores a value, but the value is not part of the identity of
  // the field, so we don't want to compare the values.
  return (label_ == field.label_ &&
          name_ == field.name_ &&
          form_control_type_ == field.form_control_type_ &&
          input_type_ == field.input_type_);
}

bool FormField::operator!=(const FormField& field) const {
  return !operator==(field);
}

std::ostream& operator<<(std::ostream& os, const FormField& field) {
  return os
      << UTF16ToUTF8(field.label())
      << " "
      << UTF16ToUTF8(field.name())
      << " "
      << UTF16ToUTF8(field.value())
      << " "
      << UTF16ToUTF8(field.form_control_type())
      << " "
      << field.input_type();
}

}  // namespace webkit_glue
