// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Installs iOS Translate callbacks on cr.googleTranslate.
 *
 * TODO(crbug.com/659442): Enable checkTypes, checkVars errors for this file.
 * @suppress {checkTypes, checkVars}
 */

/**
 * Defines function to install callbacks on cr.googleTranslate.
 * See translate_script.cc for usage.
 */
var installTranslateCallbacks = function() {
/**
 * Sets a callback to inform host of the ready state of the translate element.
 */
cr.googleTranslate.readyCallback = function() {
  __gCrWeb.message.invokeOnHost({
      'command': 'translate.ready',
      'errorCode': cr.googleTranslate.errorCode,
      'loadTime': cr.googleTranslate.loadTime,
      'readyTime': cr.googleTranslate.readyTime});
}

/**
 * Sets a callback to inform host of the result of translation.
 */
cr.googleTranslate.resultCallback = function() {
  __gCrWeb.message.invokeOnHost({
      'command': 'translate.status',
      'errorCode': cr.googleTranslate.errorCode,
      'originalPageLanguage': cr.googleTranslate.sourceLang,
      'translationTime': cr.googleTranslate.translationTime});
}

/**
 * Sets a callback to inform host to download javascript.
 */
cr.googleTranslate.loadJavascriptCallback = function(url) {
  __gCrWeb.message.invokeOnHost({
      'command': 'translate.loadjavascript',
      'url': url});
}

}  // installTranslateCallbacks
