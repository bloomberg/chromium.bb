// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CreateElementFlags_h
#define CreateElementFlags_h

#include "platform/wtf/Allocator.h"

class CreateElementFlags {
  STACK_ALLOCATED();

 public:
  bool IsCreatedByParser() const { return created_by_parser_; }
  bool IsAsyncCustomElements() const { return async_custom_elements_; }

  // https://html.spec.whatwg.org/#create-an-element-for-the-token
  static CreateElementFlags ByParser() {
    return CreateElementFlags().SetCreatedByParser();
  }

  // https://dom.spec.whatwg.org/#concept-node-clone
  static CreateElementFlags ByCloneNode() {
    return CreateElementFlags().SetAsyncCustomElements();
  }

  // https://dom.spec.whatwg.org/#dom-document-importnode
  static CreateElementFlags ByImportNode() { return ByCloneNode(); }

  // https://dom.spec.whatwg.org/#dom-document-createelement
  static CreateElementFlags ByCreateElement() { return CreateElementFlags(); }

  // https://html.spec.whatwg.org/#create-an-element-for-the-token
  static CreateElementFlags ByFragmentParser() {
    return CreateElementFlags().SetCreatedByParser().SetAsyncCustomElements();
  }

 private:
  CreateElementFlags()
      : created_by_parser_(false), async_custom_elements_(false) {}

  CreateElementFlags& SetCreatedByParser() {
    created_by_parser_ = true;
    return *this;
  }

  CreateElementFlags& SetAsyncCustomElements() {
    async_custom_elements_ = true;
    return *this;
  }

  bool created_by_parser_ : 1;
  bool async_custom_elements_ : 1;
};

#endif  // CreateElementFlags_h
