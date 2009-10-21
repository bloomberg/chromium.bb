// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
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
  for (Vector<String>::const_iterator it = charsets.begin();
       it != charsets.end(); ++it) {
    *encoding = TextEncoding(*it);
    if (encoding->isValid())
      return;
  }
  if (!encoding->isValid()) {
    Frame* frame = form->document()->frame();
    *encoding = frame ?
        TextEncoding(frame->loader()->encoding()) : Latin1Encoding();
  }
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
  WTF::Vector<HTMLFormControlElement*> form_elements = form->formElements;
  HTMLFormControlElement* first_submit_button = NULL;

  for (unsigned i = 0; i < form_elements.size(); ++i) {
    HTMLFormControlElement* current = form_elements[i];
    if (current->isActivatedSubmit()) {
      // There's a button that is already activated for submit, return NULL.
      return NULL;
    } else if (first_submit_button == NULL &&
               current->isSuccessfulSubmitButton()) {
      first_submit_button = current;
    }
  }
  return first_submit_button;
}

// Returns true if the selected state of all the options matches the default
// selected state.
bool IsSelectInDefaultState(HTMLSelectElement* select) {
  RefPtr<HTMLOptionsCollection> options = select->options();
  Node* node = options->firstItem();

  if (!select->multiple() && select->size() <= 1) {
    // The select is rendered as a combobox (called menulist in WebKit). At
    // least one item is selected, determine which one.
    HTMLOptionElement* initial_selected = NULL;
    while (node) {
      HTMLOptionElement* option_element = CastToHTMLOptionElement(node);
      if (option_element) {
        if (!initial_selected)
          initial_selected = option_element;
        if (option_element->defaultSelected()) {
          // The page specified the option to select.
          initial_selected = option_element;
          break;
        }
      }
      node = options->nextItem();
    }
    if (initial_selected)
      return initial_selected->selected();
  } else {
    while (node) {
      HTMLOptionElement* option_element = CastToHTMLOptionElement(node);
      if (option_element &&
          option_element->selected() != option_element->defaultSelected()) {
        return false;
      }
      node = options->nextItem();
    }
  }
  return true;
}

bool IsCheckBoxOrRadioInDefaultState(HTMLInputElement* element) {
  return (element->checked() == element->defaultChecked());
}

// Returns true if the form element is in its default state, false otherwise.
// The default state is the state of the form element on initial load of the
// page, and varies depending upon the form element. For example, a checkbox is
// in its default state if the checked state matches the defaultChecked state.
bool IsInDefaultState(HTMLFormControlElement* form_element) {
  if (form_element->hasTagName(HTMLNames::inputTag)) {
    HTMLInputElement* input_element =
        static_cast<HTMLInputElement*>(form_element);
    if ((input_element->inputType() == HTMLInputElement::CHECKBOX) ||
        (input_element->inputType() == HTMLInputElement::RADIO)) {
      return IsCheckBoxOrRadioInDefaultState(input_element);
    }
  } else if (form_element->hasTagName(HTMLNames::selectTag)) {
    return IsSelectInDefaultState(
        static_cast<HTMLSelectElement*>(form_element));
  }
  return true;
}

// If form has only one text input element, it is returned. If a valid input
// element is not found, NULL is returned. Additionally, the form data for all
// elements is added to enc_string and the encoding used is set in
// encoding_name.
HTMLInputElement* GetTextElement(HTMLFormElement* form,
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
  WTF::Vector<HTMLFormControlElement*> form_elements = form->formElements;
  for (unsigned i = 0; i < form_elements.size(); ++i) {
    HTMLFormControlElement* form_element = form_elements[i];
    if (!form_element->disabled() && !form_element->name().isNull()) {
      bool is_text_element = false;
      if (!IsInDefaultState(form_element))
        return NULL;
      if (form_element->hasTagName(HTMLNames::inputTag)) {
        HTMLInputElement* input_element =
            static_cast<HTMLInputElement*>(form_element);
        switch (input_element->inputType()) {
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
            break;
          default:
            // All other input types are indexable.
            break;
        }
      } else if (form_element->hasTagName(HTMLNames::textareaTag)) {
        // Textareas aren't used for search.
        return NULL;
      }
      FormDataList lst(encoding);
      if (form_element->appendFormData(lst, false)) {
        if (is_text_element && !lst.list().isEmpty()) {
          if (text_element != NULL) {
            // The auto-complete bar only knows how to fill in one value.
            // This form has multiple fields; don't treat it as searchable.
            return NULL;
          }
          text_element = static_cast<HTMLInputElement*>(form_element);
        }
        for (int j = 0, max = static_cast<int>(lst.list().size()); j < max;
             ++j) {
          const FormDataList::Item& item = lst.list()[j];
          // handle ISINDEX / <input name=isindex> special
          // but only if its the first entry
          if (enc_string->isEmpty() && item.data() == "isindex") {
            if (form_element == text_element) {
              enc_string->append("{searchTerms}", 13);
            } else {
              FormDataBuilder::encodeStringAsFormData(
                  *enc_string, (lst.list()[j + 1].data()));
            }
            ++j;
          } else {
            if (!enc_string->isEmpty())
              enc_string->append('&');
            FormDataBuilder::encodeStringAsFormData(*enc_string, item.data());
            enc_string->append('=');
            if (form_element == text_element) {
              enc_string->append("{searchTerms}", 13);
            } else {
              FormDataBuilder::encodeStringAsFormData(
                  *enc_string, lst.list()[j + 1].data());
            }
            ++j;
          }
        }
      }
    }
  }
  return text_element;
}

} // namespace

SearchableFormData* SearchableFormData::Create(const WebKit::WebForm& webform) {
  RefPtr<HTMLFormElement> form = WebFormToHTMLFormElement(webform);

  Frame* frame = form->document()->frame();
  if (frame == NULL)
    return NULL;

  // Only consider forms that GET data and the action targets an http page.
  if (equalIgnoringCase(form->getAttribute(HTMLNames::methodAttr), "post") ||
      !IsHTTPFormSubmit(form.get()))
    return NULL;

  Vector<char> enc_string;
  HTMLFormControlElement* first_submit_button = GetButtonToActivate(form.get());

  if (first_submit_button) {
    // The form does not have an active submit button, make the first button
    // active. We need to do this, otherwise the URL will not contain the
    // name of the submit button.
    first_submit_button->setActivatedSubmit(true);
  }

  std::string encoding;
  HTMLInputElement* text_element =
      GetTextElement(form.get(), &enc_string, &encoding);

  if (first_submit_button)
    first_submit_button->setActivatedSubmit(false);

  if (text_element == NULL) {
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
