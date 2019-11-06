// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_BUTTON_TITLE_TYPE_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_BUTTON_TITLE_TYPE_H_

namespace autofill {

// Describes how a form button is implemented in HTML source. Should be
// synced with |ButtonTitleType| in
// components/autofill/core/browser/proto/server.proto.
enum ButtonTitleType {
  NONE = 0,
  BUTTON_ELEMENT_SUBMIT_TYPE = 1,  // <button type='submit'>
  BUTTON_ELEMENT_BUTTON_TYPE = 2,  // <button type='button'>
  INPUT_ELEMENT_SUBMIT_TYPE = 3,   // <input type='submit'>
  INPUT_ELEMENT_BUTTON_TYPE = 4,   // <input type='button'>
  HYPERLINK = 5,                   // e.g. <a class='button'>
  DIV = 6,                         // e.g. <div id='submit'>
  SPAN = 7                         // e.g. <span name='btn'>
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_FORM_SUBMISSION_SOURCE_H_
