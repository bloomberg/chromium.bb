// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for stuff not added to the Closure compiler yet, but
 * should get added.
 */

/**
 * @see https://developer.mozilla.org/en-US/docs/Web/API/Element/setPointerCapture
 * @param {number} pointerId
 * TODO(dpapad): Remove this once it is added to Closure Compiler itself.
 */
Element.prototype.setPointerCapture = function(pointerId) {};
