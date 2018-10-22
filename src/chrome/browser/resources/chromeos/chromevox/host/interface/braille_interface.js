// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a Braille interface.
 *
 * All Braille engines in ChromeVox conform to this interface.
 *
 */

goog.provide('cvox.BrailleInterface');

goog.require('cvox.BrailleKeyCommand');
goog.require('cvox.BrailleKeyEvent');
goog.require('cvox.NavBraille');

/**
 * @interface
 */
cvox.BrailleInterface = function() {};

/**
 * Sends the given params to the Braille display for output.
 * @param {!cvox.NavBraille} params Parameters to send to the
 * platform braille service.
 */
cvox.BrailleInterface.prototype.write = function(params) {};

/**
 * Takes an image in the form of a data url and outputs it to a Braille
 * display.
 * @param {!string} imageDataUrl The image to output, in the form of a
 * dataUrl.
 */
cvox.BrailleInterface.prototype.writeRawImage = function(imageDataUrl) {};

/**
 * Freeze whatever is on the braille display until the next call to thaw().
 */
cvox.BrailleInterface.prototype.freeze = function() {};


/**
 * Un-freeze the braille display so that it can be written to again.
 */
cvox.BrailleInterface.prototype.thaw = function() {};


/**
 * @return {!cvox.BrailleDisplayState} The current display state.
 */
cvox.BrailleInterface.prototype.getDisplayState = function() {};
