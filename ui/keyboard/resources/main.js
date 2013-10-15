// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var onResize = function() {
  var x = window.innerWidth;
  var y = window.innerHeight;
  var height =  (x > ASPECT_RATIO * y) ? y : Math.floor(x / ASPECT_RATIO);
  keyboard.style.height = height + 'px';
  keyboard.style.width = Math.floor(ASPECT_RATIO * height) + 'px';
  keyboard.style.fontSize = (height / FONT_SIZE_RATIO / ROW_LENGTH) + 'px';
};

/**
 * Recursively replace all kb-key-import elements with imported documents.
 * @param {!Document} content Document to process.
 */
function importHTML(content) {
  var dom = content.querySelector('template').createInstance();
  var keyImports = dom.querySelectorAll('kb-key-import');
  if (keyImports.length != 0) {
    keyImports.forEach(function(element) {
      if (element.importDoc(content)) {
        var generatedDom = importHTML(element.importDoc(content));
        element.parentNode.replaceChild(generatedDom, element);
      }
    });
  }
  return dom;
}

/**
 * Replace all kb-key-sequence elements with generated kb-key elements.
 * @param {!DocumentFragment} importedContent The imported dom structure.
 */
function expandHTML(importedContent) {
  var keySequences = importedContent.querySelectorAll('kb-key-sequence');
  if (keySequences.length != 0) {
    keySequences.forEach(function(element) {
      var generatedDom = element.generateDom();
      element.parentNode.replaceChild(generatedDom, element);
    });
  }
}

/**
  * Flatten the keysets which represents a keyboard layout. It has two steps:
  * 1) Replace all kb-key-import elements with imported document that associated
  *   with linkid.
  * 2) Replace all kb-key-sequence elements with generated DOM structures.
  * @param {!Document} content Document to process.
  */
function flattenKeysets(content) {
  var importedContent = importHTML(content);
  expandHTML(importedContent);
  return importedContent;
}

addEventListener('resize', onResize);

addEventListener('load', onResize);

// Prevents all default actions of touch. Keyboard should use its own gesture
// recognizer.
addEventListener('touchstart', function(e) { e.preventDefault() });
addEventListener('touchend', function(e) { e.preventDefault() });
addEventListener('touchmove', function(e) { e.preventDefault() });
