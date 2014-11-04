// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @struct
 */
function TextSearchState() {
  /**
   * @type {string}
   */
  this.text = '';

  /**
   * @type {!Date}
   */
  this.date = new Date();
}

/**
 * List container for the file table and the grid view.
 * @param {!HTMLElement} element Element of the container.
 * @constructor
 * @struct
 */
function ListContainer(element) {
  /**
   * The container element of the file list.
   * @type {!HTMLElement}
   * @const
   */
  this.element = element;

  /**
   * Spinner on file list which is shown while loading.
   * @type {!HTMLElement}
   * @const
   */
  this.spinner = queryRequiredElement(element, '.spinner-layer');

  /**
   * @type {!TextSearchState}
   * @const
   */
  this.textSearchState = new TextSearchState();

  this.element.addEventListener('keydown', this.onKeyDown_.bind(this));
  this.element.addEventListener('keypress', this.onKeyPress_.bind(this));
  this.element.addEventListener('mousemove', this.onMouseMove_.bind(this));
}

/**
 * @enum {string}
 * @const
 */
ListContainer.EventType = {
  TEXT_SEARCH: 'textsearch'
};

/**
 * Clears hover highlighting in the list container until next mouse move.
 */
ListContainer.prototype.clearHover = function() {
  this.element.classList.add('nohover');
};

/**
 * KeyDown event handler for the div#list-container element.
 * @param {!Event} event Key event.
 * @private
 */
ListContainer.prototype.onKeyDown_ = function(event) {
  // Ignore keydown handler in the rename input box.
  if (event.srcElement.tagName == 'INPUT') {
    event.stopImmediatePropagation();
    return;
  }

  switch (event.keyIdentifier) {
    case 'Home':
    case 'End':
    case 'Up':
    case 'Down':
    case 'Left':
    case 'Right':
      // When navigating with keyboard we hide the distracting mouse hover
      // highlighting until the user moves the mouse again.
      this.clearHover();
      break;
  }
};

/**
 * KeyPress event handler for the div#list-container element.
 * @param {!Event} event Key event.
 * @private
 */
ListContainer.prototype.onKeyPress_ = function(event) {
  // Ignore keypress handler in the rename input box.
  if (event.srcElement.tagName == 'INPUT' ||
      event.ctrlKey ||
      event.metaKey ||
      event.altKey) {
    event.stopImmediatePropagation();
    return;
  }

  var now = new Date();
  var character = String.fromCharCode(event.charCode).toLowerCase();
  var text = now - this.textSearchState.date > 1000 ? '' :
      this.textSearchState.text;
  this.textSearchState.text = text + character;
  this.textSearchState.date = now;

  if (this.textSearchState.text)
    cr.dispatchSimpleEvent(this.element, ListContainer.EventType.TEXT_SEARCH);
};

/**
 * Mousemove event handler for the div#list-container element.
 * @param {Event} event Mouse event.
 * @private
 */
ListContainer.prototype.onMouseMove_ = function(event) {
  // The user grabbed the mouse, restore the hover highlighting.
  this.element.classList.remove('nohover');
};
