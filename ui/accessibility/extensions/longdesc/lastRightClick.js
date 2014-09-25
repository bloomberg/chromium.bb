/* Copyright (c) 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

var borderColor;
var borderStyle;
var borderWidth;

chrome.storage.onChanged.addListener(function(changes, namespace) {
  if (changes.addBorder.newValue) {
    addBorders();
  } else {
    removeBorders();
  }
});

chrome.storage.sync.get("addBorder", function(item) {
  if (item.addBorder) {
    addBorders();
  }
});

document.addEventListener('contextmenu', function(element) {
  updateContextMenuItem(element);
}, false);

document.addEventListener('mouseover', function(element) {
  updateContextMenuItem(element);
}, false);

document.addEventListener('focus', function(element) {
  updateContextMenuItem(element);
});

/**
 * Sends a message to the backgrond script notifying it to
 * enable or disable the context menu item.
 *
 * @param element
 */
function updateContextMenuItem(element) {
  var longDesc = '';
  var ariaDescribedAt = '';

  if (element.target.hasAttribute("longdesc")) {
    longDesc = element.target.getAttribute("longdesc");
  }

  if (element.target.hasAttribute("aria-describedat")) {
    ariaDescribedAt = element.target.getAttribute("aria-describedat");
  }

  if (longDesc !== '' || ariaDescribedAt !== '') {
    chrome.runtime.sendMessage({
      ariaDescribedAt: ariaDescribedAt,
      longDesc: longDesc,
      enabled: true
    });
  } else {
    chrome.runtime.sendMessage({
      enabled: false
    });
  }
}

/**
 * Modify border to make the HTML element more visible.
 */
function addBorders() {
  var elementArray = new Array(document.querySelectorAll('[longdesc]'));
  elementArray.concat(new Array(document.querySelectorAll('[aria-describedat]')));

  for (var i = 0; i < elementArray.length; i++) {
    borderColor = elementArray[0][i].style.borderColor;
    borderStyle = elementArray[0][i].style.borderStyle;
    borderWidth = elementArray[0][i].style.borderWidth;

    elementArray[0][i].style.borderColor = 'blue';
    elementArray[0][i].style.borderStyle = 'groove';
    elementArray[0][i].style.borderWidth = '15px';
  }
}

/**
 * Revert back to the original border styling.
 */
function removeBorders() {
  var elementArray = new Array(document.querySelectorAll('[longdesc]'));
  elementArray.concat(new Array(document.querySelectorAll('[aria-describedat]')));

  for (var i = 0; i < elementArray.length; i++) {
    elementArray[0][i].style.borderColor = borderColor;
    elementArray[0][i].style.borderStyle = borderStyle;
    elementArray[0][i].style.borderWidth = borderWidth;
  }
}
