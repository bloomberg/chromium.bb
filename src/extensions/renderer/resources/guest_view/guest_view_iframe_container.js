// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// GuestViewCrossProcessFrames overrides for guest_view_container.js

var $Document = require('safeMethods').SafeMethods.$Document;
var $HTMLElement = require('safeMethods').SafeMethods.$HTMLElement;
var $Node = require('safeMethods').SafeMethods.$Node;
var GuestViewContainer = require('guestViewContainer').GuestViewContainer;
var IdGenerator = requireNative('id_generator');

GuestViewContainer.prototype.createInternalElement$ = function() {
  var iframeElement = $Document.createElement(document, 'iframe');

  var style = $HTMLElement.style.get(iframeElement);
  $Object.defineProperty(style, 'width', {value: '100%'});
  $Object.defineProperty(style, 'height', {value: '100%'});
  $Object.defineProperty(style, 'border', {value: '0px'});

  return iframeElement;
};

GuestViewContainer.prototype.prepareForReattach$ = function() {
  // Since attachment swaps a local frame for a remote frame, we need our
  // internal iframe element to be local again before we can reattach.
  var newFrame = this.createInternalElement$();
  var oldFrame = this.internalElement;
  this.internalElement = newFrame;
  var frameParent = $Node.parentNode.get(oldFrame);
  $Node.replaceChild(frameParent, newFrame, oldFrame);
};

GuestViewContainer.prototype.attachWindow$ = function() {
  var generatedId = IdGenerator.GetNextId();
  // Generate an instance id for the container.
  this.onInternalInstanceId(generatedId);
  return true;
};

GuestViewContainer.prototype.willAttachElement$ = function() {
  if (this.deferredAttachCallback) {
    this.deferredAttachCallback();
    this.deferredAttachCallback = null;
  }
};
