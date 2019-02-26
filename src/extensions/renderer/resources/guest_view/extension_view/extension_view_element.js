// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The <extensionview> custom element.

var registerElement = require('guestViewContainerElement').registerElement;
var forwardApiMethods = require('guestViewContainerElement').forwardApiMethods;
var GuestViewContainerElement =
    require('guestViewContainerElement').GuestViewContainerElement;
var ExtensionViewImpl = require('extensionView').ExtensionViewImpl;
var ExtensionViewConstants =
    require('extensionViewConstants').ExtensionViewConstants;
var EXTENSION_VIEW_API_METHODS =
    require('extensionViewApiMethods').EXTENSION_VIEW_API_METHODS;

class ExtensionViewElement extends GuestViewContainerElement {
  static get observedAttributes() {
    return [
      ExtensionViewConstants.ATTRIBUTE_EXTENSION,
      ExtensionViewConstants.ATTRIBUTE_SRC
    ];
  }

  constructor() {
    super();
    privates(this).internal = new ExtensionViewImpl(this);
  }
}

// Forward ExtensionViewElement.foo* method calls to ExtensionViewImpl.foo*.
forwardApiMethods(
    ExtensionViewElement, ExtensionViewImpl, null, EXTENSION_VIEW_API_METHODS);

registerElement('ExtensionView', ExtensionViewElement);
