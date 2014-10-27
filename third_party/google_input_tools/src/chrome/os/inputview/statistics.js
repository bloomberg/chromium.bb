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
goog.provide('i18n.input.chrome.inputview.Statistics');



/**
 * The statistics util class for input view.
 *
 * @constructor
 */
i18n.input.chrome.inputview.Statistics = function() {
};
goog.addSingletonGetter(i18n.input.chrome.inputview.Statistics);


/**
 * The current layout code.
 *
 * @type {string}
 * @private
 */
i18n.input.chrome.inputview.Statistics.prototype.layoutCode_ = '';


/**
 * Sets the current layout code, which will be a part of the metrics name.
 *
 * @param {string} layoutCode .
 */
i18n.input.chrome.inputview.Statistics.prototype.setCurrentLayout = function(
    layoutCode) {
  this.layoutCode_ = layoutCode;
};


/**
 * Gets the target type based on the given source and target.
 *
 * @param {string} source .
 * @param {string} target .
 * @return {number} The target type number value.
 */
i18n.input.chrome.inputview.Statistics.prototype.getTargetType = function(
    source, target) {
  if (source == target) {
    return 0;
  }
  if (!source) {
    return 3;
  }
  if (target.length > source.length) {
    return 2;
  }
  return 1;
};


/**
 * Records the metrics for each commit.
 *
 * @param {number} sourceLen The source length.
 * @param {number} targetLen The target length.
 * @param {number} targetIndex The target index.
 * @param {number} targetType The target type:
 *     0: Source; 1: Correction; 2: Completion; 3: Prediction.
 * @param {number} triggerType The trigger type:
 *     0: BySpace; 1: ByReset; 2: ByCandidate; 3: BySymbolOrNumber;
 *     4: ByDoubleSpaceToPeriod.
 */
i18n.input.chrome.inputview.Statistics.prototype.recordCommit = function(
    sourceLen, targetLen, targetIndex, targetType, triggerType) {
  if (!this.layoutCode_) {
    return;
  }
  this.recordCount_('Commit.SourceLength', sourceLen);
  this.recordCount_('Commit.TargetLength', targetLen);
  this.recordCount_('Commit.TargetIndex', targetIndex + 1); // 1-based index.
  this.recordCount_('Commit.TargetType', targetType);
  this.recordCount_('Commit.TriggerType', triggerType);
};


/**
 * Records the VK show time for current keyboard layout.
 *
 * @param {number} showTimeSeconds The show time in seconds.
 */
i18n.input.chrome.inputview.Statistics.prototype.recordShowTime = function(
    showTimeSeconds) {
  this.recordCount_('Layout.ShowTime', showTimeSeconds);
};


/**
 * Records the layout switching action.
 *
 * @param {string} layoutCode The layout code to be switched to.
 */
i18n.input.chrome.inputview.Statistics.prototype.recordSwitch = function(
    layoutCode) {
  this.recordCount_('Layout.SwitchTo.' + layoutCode, 1);
};


/**
 * Records the special key action, which is function key like BACKSPACE, ENTER,
 * etc..
 *
 * @param {string} keyCode .
 * @param {boolean} isComposing Whether the key is pressed with composing mode.
 */
i18n.input.chrome.inputview.Statistics.prototype.recordSpecialKey = function(
    keyCode, isComposing) {
  var name = 'Key.' + isComposing ? 'Composing.' : '' + keyCode;
  this.recordCount_(name, 1);
};


/**
 * Records count value to chrome UMA framework.
 *
 * @param {string} name .
 * @param {number} count .
 * @private
 */
i18n.input.chrome.inputview.Statistics.prototype.recordCount_ = function(
    name, count) {
  if (!this.layoutCode_) {
    return;
  }
  if (chrome.metricsPrivate && chrome.metricsPrivate.recordCount) {
    name = 'InputView.' + name + '.' + this.layoutCode_;
    chrome.metricsPrivate.recordCount(name, count);
  }
};
