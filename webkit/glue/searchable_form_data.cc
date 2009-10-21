// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "Document.h"
#include "FormDataBuilder.h"
#include "FormDataList.h"
#include "Frame.h"
#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"
#include "HTMLOptionElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLOptionsCollection.h"
#include "HTMLSelectElement.h"
#include "TextEncoding.h"
MSVC_POP_WARNING();

#undef LOG

#include "webkit/glue/dom_operations_private.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/searchable_form_data.h"

using namespace WebCore;

namespace webkit_glue {

namespace {

// Gets the encoding for the form.
void GetFormEncoding(HTMLFormElement* form, TextEncoding* encoding) {
  String str(form->getAttribute(HTMLNames::accept_charsetAttr));
  str.replace(',', ' ');
  Vector<String> charsets;
  str.split(' ', charsets);
  for (Vector<String>::const_iterator i(charsets.begin()); i != charsets.end();
       ++i) {
    *encoding = TextEncoding(*i);
    if (encoding->isValid())
      return;
  }
  const Frame* frame = form->document()->frame();
  *encoding = frame ?
      TextEncoding(frame->loader()->encoding()) : Latin1Encoding();
}

// Returns true if the submit request results in an HTTP URL.
bool IsHTTPFormSubmit(HTMLFormElement* form) {
  String action(form->action());
  return form->document()->frame()->loader()->
      completeURL(action.isNull() ? "" : action).protocol() == "http";
}

// If the form does not have an activated submit button, the first submit
// button is returned.
HTMLFormControlElement* GetButtonToActivate(HTMLFormElement* form) {
  HTMLFormControlElement* first_submit_button = NULL;
  for (WTF::Vector<HTMLFormControlElement*>::const_iterator
       i(form->formElements.begin()); i != form->formElements.end(); ++i) {
    HTMLFormControlElement* form_element = *i;
    if (form_element->isActivatedSubmit()) {
      // There's a button that is already activated for submit, return NULL.
      return NULL;
    } else if (first_submit_button == NULL &&
               form_element->isSuccessfulSubmitButton()) {
      first_submit_button = form_element;
    }
  }
  return first_submit_button;
}

// Returns true if the selected state of all the options matches the default
// selected state.
bool IsSelectInDefaultState(HTMLSelectElement* select) {
  RefPtr<HTMLOptionsCollection> options = select->options();
  if (select->multiple() || select->size() > 1) {
    for (Node* node = options->firstItem(); node; node = options->nextItem()) {
      const HTMLOptionElement* option_element = CastToHTMLOptionElement(node);
      if (option_element &&
          option_element->selected() != option_element->defaultSelected()) {
        return false;
      }
    }
    return true;
  }

  // The select is rendered as a combobox (called menulist in WebKit). At
  // least one item is selected, determine which one.
  const HTMLOptionElement* initial_selected = NULL;
  for (Node* node = options->firstItem(); node; node = options->nextItem()) {
    const HTMLOptionElement* option_element = CastToHTMLOptionElement(node);
    if (!option_element)
      continue;
    if (option_element->defaultSelected()) {
      // The page specified the option to select.
      initial_selected = option_element;
      break;
    } else if (!initial_selected) {
      initial_selected = option_element;
    }
  }
  return initial_selected ? initial_selected->selected() : true;
}

// Returns true if the form element is in its default state, false otherwise.
// The default state is the state of the form element on initial load of the
// page, and varies depending upon the form element. For example, a checkbox is
// in its default state if the checked state matches the defaultChecked state.
bool IsInDefaultState(HTMLFormControlElement* form_element) {
  if (form_element->hasTagName(HTMLNames::inputTag)) {
    const HTMLInputElement* input_element =
        static_cast<const HTMLInputElement*>(form_element);
    if (input_element->inputType() == HTMLInputElement::CHECKBOX ||
        input_element->inputType() == HTMLInputElement::RADIO) {
      return input_element->checked() == input_element->defaultChecked();
    }
  } else if (form_element->hasTagName(HTMLNames::selectTag)) {
    return IsSelectInDefaultState(
        static_cast<HTMLSelectElement*>(form_element));
  }
  return true;
}

// If form has only one text input element, return true. If a valid input
// element is not found, return false. Additionally, the form data for all
// elements is added to enc_string and the encoding used is set in
// encoding_name.
bool HasSuitableTextElement(HTMLFormElement* form,
                            Vector<char>* enc_string,
                            std::string* encoding_name) {
  TextEncoding encoding;
  GetFormEncoding(form, &encoding);
  if (!encoding.isValid()) {
    // Need a valid encoding to encode the form elements.
    // If the encoding isn't found webkit ends up replacing the params with
    // empty strings. So, we don't try to do anything here.
    return NULL;
  }
  *encoding_name = encoding.name();

  HTMLInputElement* text_element = NULL;
  for (WTF::Vector<HTMLFormControlElement*>::const_iterator
       i(form->formElements.begin()); i != form->formElements.end(); ++i) {
    HTMLFormControlElement* form_element = *i;
    if (form_element->disabled() || form_element->name().isNull())
      continue;

    if (!IsInDefaultState(form_element) ||
        form_element->hasTagName(HTMLNames::textareaTag))
      return NULL;

    bool is_text_element = false;
    if (form_element->hasTagName(HTMLNames::inputTag)) {
      switch (static_cast<const HTMLInputElement*>(form_element)->inputType()) {
        case HTMLInputElement::TEXT:
        case HTMLInputElement::ISINDEX:
          is_text_element = true;
          break;
        case HTMLInputElement::PASSWORD:
          // Don't store passwords! This is most likely an https anyway.
          // Fall through.
        case HTMLInputElement::FILE:
          // Too big, don't try to index this.
          return NULL;
        default:
          // All other input types are indexable.
          break;
      }
    }

    FormDataList data_list(encoding);
    if (!form_element->appendFormData(data_list, false))
      continue;

    const WTF::Vector<FormDataList::Item>& item_list = data_list.list();
    if (is_text_element && !item_list.isEmpty()) {
      if (text_element != NULL) {
        // The auto-complete bar only knows how to fill in one value.
        // This form has multiple fields; don't treat it as searchable.
        return false;
      }
      text_element = static_cast<HTMLInputElement*>(form_element);
    }
    for (WTF::Vector<FormDataList::Item>::const_iterator j(item_list.begin());
         j != item_list.end(); ++j) {
      // Handle ISINDEX / <input name=isindex> specially, but only if it's
      // the first entry.
      if (!enc_string->isEmpty() || j->data() != "isindex") {
        if (!enc_string->isEmpty())
          enc_string->append('&');
        FormDataBuilder::encodeStringAsFormData(*enc_string, j->data());
        enc_string->append('=');
      }
      ++j;
      if (form_element == text_element)
        enc_string->append("{searchTerms}", 13);
      else
        FormDataBuilder::encodeStringAsFormData(*enc_string, j->data());
    }
  }

  return text_element != NULL;
}

} // namespace

SearchableFormData* SearchableFormData::Create(const WebKit::WebForm& webform) {
  RefPtr<HTMLFormElement> form = WebFormToHTMLFormElement(webform);
  const Frame* frame = form->document()->frame();
  if (frame == NULL)
    return NULL;

  // Only consider forms that GET data and the action targets an http page.
  if (equalIgnoringCase(form->getAttribute(HTMLNames::methodAttr), "post") ||
      !IsHTTPFormSubmit(form.get()))
    return NULL;

  HTMLFormControlElement* first_submit_button = GetButtonToActivate(form.get());
  if (first_submit_button) {
    // The form does not have an active submit button, make the first button
    // active. We need to do this, otherwise the URL will not contain the
    // name of the submit button.
    first_submit_button->setActivatedSubmit(true);
  }
  Vector<char> enc_string;
  std::string encoding;
  bool has_element = HasSuitableTextElement(form.get(), &enc_string, &encoding);
  if (first_submit_button)
    first_submit_button->setActivatedSubmit(false);
  if (!has_element) {
    // Not a searchable form.
    return NULL;
  }

  // It's a valid form.
  // Generate the URL and create a new SearchableFormData.
  RefPtr<FormData> form_data = FormData::create(enc_string);
  String action(form->action());
  KURL url = frame->loader()->completeURL(action.isNull() ? "" : action);
  url.setQuery(form_data->flattenToString());
  GURL gurl(KURLToGURL(url));
  return new SearchableFormData(gurl, encoding);
}

}  // namespace webkit_glue
