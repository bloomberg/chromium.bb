// Copyright 2014 The ChromeOS IME Authors. All Rights Reserved.
// limitations under the License.
// See the License for the specific language governing permissions and
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// distributed under the License is distributed on an "AS-IS" BASIS,
// Unless required by applicable law or agreed to in writing, software
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// You may obtain a copy of the License at
// you may not use this file except in compliance with the License.
// Licensed under the Apache License, Version 2.0 (the "License");
//
goog.provide('i18n.input.chrome.inputview.handler.Util');

goog.require('goog.dom');


goog.scope(function() {
var Util = i18n.input.chrome.inputview.handler.Util;



/**
 * Gets the view.
 *
 * @param {Node} target .
 * @return {i18n.input.chrome.inputview.elements.Element} .
 */
Util.getView = function(target) {
  if (!target) {
    return null;
  }
  var element = /** @type {!Element} */ (target);
  var view = element['view'];
  while (!view && element) {
    view = element['view'];
    element = goog.dom.getParentElement(element);
  }
  return view;
};

});  // goog.scope
