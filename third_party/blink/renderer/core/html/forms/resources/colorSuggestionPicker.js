'use strict';
/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */


function initializeColorSuggestionPicker() {
  new ColorSuggestionPicker(main, global.params);
}

/**
 * @param {!Object} args
 * @return {?string} An error message, or null if the argument has no errors.
 */
function validateColorSuggestionPickerArguments(args) {
  if (!args.shouldShowColorSuggestionPicker)
    return 'Should not be showing the color suggestion picker.'
  if (!args.values)
    return 'No values.';
  if (!args.otherColorLabel)
    return 'No otherColorLabel.';
  return null;
}

function handleArgumentsTimeout() {
  if (global.argumentsReceived)
    return;
  var args = {values: DefaultColorPalette, otherColorLabel: 'Other...'};
  initialize(args);
}

function ColorSuggestionPicker(element, config) {
  Picker.call(this, element, config);
  this._config = config;
  if (this._config.values.length === 0)
    this._config.values = DefaultColorPalette;
  this._container = null;
  this._layout();
  document.body.addEventListener('keydown', this._handleKeyDown.bind(this));
  this._element.addEventListener('mousemove', this._handleMouseMove.bind(this));
  this._element.addEventListener('mousedown', this._handleMouseDown.bind(this));
}
ColorSuggestionPicker.prototype = Object.create(Picker.prototype);

var SwatchBorderBoxWidth = 24;   // keep in sync with CSS
var SwatchBorderBoxHeight = 24;  // keep in sync with CSS
var SwatchesPerRow = 5;
var SwatchesMaxRow = 4;

ColorSuggestionPicker.prototype._layout = function() {
  var container = createElement('div', 'color-swatch-container');
  container.addEventListener('click', this._handleSwatchClick.bind(this), false);
  for (var i = 0; i < this._config.values.length; ++i) {
    var swatch = createElement('button', 'color-swatch');
    swatch.dataset.index = i;
    swatch.dataset.value = this._config.values[i];
    swatch.title = this._config.values[i];
    swatch.style.backgroundColor = this._config.values[i];
    container.appendChild(swatch);
  }
  var containerWidth = SwatchBorderBoxWidth * SwatchesPerRow;
  if (this._config.values.length > SwatchesPerRow * SwatchesMaxRow)
    containerWidth += getScrollbarWidth();
  container.style.width = containerWidth + 'px';
  container.style.maxHeight = (SwatchBorderBoxHeight * SwatchesMaxRow) + 'px';
  this._element.appendChild(container);
  var otherButton = createElement('button', 'other-color', this._config.otherColorLabel);
  otherButton.addEventListener('click', this._onOtherButtonClick.bind(this), false);
  this._element.appendChild(otherButton);
  this._container = container;
  this._otherButton = otherButton;
  var elementWidth = this._element.offsetWidth;
  var elementHeight = this._element.offsetHeight;
  resizeWindow(elementWidth, elementHeight);
};

ColorSuggestionPicker.prototype._onOtherButtonClick = function() {
  if (global.params.isFormControlsRefreshEnabled) {
    var main = $('main');
    main.innerHTML = '';
    main.classList.remove('color-suggestion-picker-main');
    main.classList.add('color-picker-main');
    // Replace document.body with a deep clone to drop all event listeners.
    var oldBody = document.body;
    var newBody = oldBody.cloneNode(true);
    oldBody.parentElement.replaceChild(newBody, oldBody);
    initializeColorPicker();
  } else {
    this.chooseOtherColor();
  }
}

ColorSuggestionPicker.prototype.selectColorAtIndex = function(index) {
  index = Math.max(Math.min(this._container.childNodes.length - 1, index), 0);
  this._container.childNodes[index].focus();
};

ColorSuggestionPicker.prototype._handleMouseMove = function(event) {
  if (event.target.classList.contains('color-swatch'))
    event.target.focus();
};

ColorSuggestionPicker.prototype._handleMouseDown = function(event) {
  // Prevent blur.
  if (event.target.classList.contains('color-swatch'))
    event.preventDefault();
};

ColorSuggestionPicker.prototype._handleKeyDown = function(event) {
  var key = event.key;
  if (key === 'Escape')
    this.handleCancel();
  else if (key == 'ArrowLeft' || key == 'ArrowUp' || key == 'ArrowRight' || key == 'ArrowDown') {
    var selectedElement = document.activeElement;
    var index = 0;
    if (selectedElement.classList.contains('other-color')) {
      if (key != 'ArrowRight' && key != 'ArrowUp')
        return;
      index = this._container.childNodes.length - 1;
    } else if (selectedElement.classList.contains('color-swatch')) {
      index = parseInt(selectedElement.dataset.index, 10);
      switch (key) {
        case 'ArrowLeft':
          index--;
          break;
        case 'ArrowRight':
          index++;
          break;
        case 'ArrowUp':
          index -= SwatchesPerRow;
          break;
        case 'ArrowDown':
          index += SwatchesPerRow;
          break;
      }
      if (index > this._container.childNodes.length - 1) {
        this._otherButton.focus();
        return;
      }
    }
    this.selectColorAtIndex(index);
  }
  event.preventDefault();
};

ColorSuggestionPicker.prototype._handleSwatchClick = function(event) {
  if (event.target.classList.contains('color-swatch'))
    this.submitValue(event.target.dataset.value);
};
