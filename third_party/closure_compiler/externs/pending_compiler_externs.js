// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Temporary externs until compiler/compiler.jar catches up.
 * @externs
 */

/**
 * @see http://dom.spec.whatwg.org/#dom-domimplementation-createhtmldocument
 * @param {string=} opt_title The title to give the new HTML document.
 * @return {!HTMLDocument}
 */
DOMImplementation.prototype.createHTMLDocument = function(opt_title) {};
