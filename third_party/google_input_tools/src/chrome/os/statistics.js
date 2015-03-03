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
goog.provide('i18n.input.chrome.Statistics');

goog.scope(function() {



/**
 * The statistics util class for IME of ChromeOS.
 *
 * @constructor
 */
i18n.input.chrome.Statistics = function() {
};
goog.addSingletonGetter(i18n.input.chrome.Statistics);
var Statistics = i18n.input.chrome.Statistics;


/**
 * The layout types for stats.
 *
 * @enum {number}
 */
Statistics.LayoutTypes = {
  COMPACT: 0,
  COMPACT_SYMBOL: 1,
  COMPACT_MORE: 2,
  FULL: 3,
  A11Y: 4,
  HANDWRITING: 5,
  EMOJI: 6,
  MAX: 7
};


/**
 * The commit type for stats.
 *
 * @enum {number}
 */
Statistics.CommitTypes = {
  X_X0: 0, // User types X, and chooses X as top suggestion.
  X_X1: 1, // User types X, and chooses X as non-top suggestion.
  X_Y0: 2, // User types X, and chooses Y as top suggestion.
  X_Y1: 3, // User types X, and chooses Y as non-top suggestion.
  PREDICTION: 4,
  REVERT: 5,
  VOICE: 6,
  MAX: 7
};


/**
 * The current input method id.
 *
 * @private {string}
 */
Statistics.prototype.inputMethodId_ = '';


/**
 * The current auto correct level.
 *
 * @private {number}
 */
Statistics.prototype.autoCorrectLevel_ = 0;


/**
 * Number of characters entered between each backspace.
 *
 * @private {number}
 */
Statistics.prototype.charactersBetweenBackspaces_ = 0;


/**
 * Whether recording for physical keyboard specially.
 *
 * @private {boolean}
 */
Statistics.prototype.isPhysicalKeyboard_ = false;


/**
 * The length of the last text commit.
 *
 * @private {number}
 */
Statistics.prototype.lastCommitLength_ = 0;


/**
 * The number of characters typed in this session.
 *
 * @private {number}
 */
Statistics.prototype.charactersCommitted_ = 0;


/**
 * Sets whether recording for physical keyboard.
 *
 * @param {boolean} isPhysicalKeyboard .
 */
Statistics.prototype.setPhysicalKeyboard = function(isPhysicalKeyboard) {
  this.isPhysicalKeyboard_ = isPhysicalKeyboard;
};


/**
 * Sets the current input method id.
 *
 * @param {string} inputMethodId .
 */
Statistics.prototype.setInputMethodId = function(
    inputMethodId) {
  this.inputMethodId_ = inputMethodId;
};


/**
 * Sets the current auto-correct level.
 *
 * @param {number} level .
 */
Statistics.prototype.setAutoCorrectLevel = function(
    level) {
  this.autoCorrectLevel_ = level;
  this.recordEnum('InputMethod.AutoCorrectLevel', level, 3);
};


/**
 * Gets the commit target type based on the given source and target.
 *
 * @param {string} source .
 * @param {string} target .
 * @return {number} The target type number value.
 *     0: Source; 1: Correction; 2: Completion; 3: Prediction.
 * @private
 */
Statistics.prototype.getTargetType_ = function(
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
 * Records that the controller session ended.
 */
Statistics.prototype.recordSessionEnd = function() {
  // Do not record cases where we gain and immediately lose focus. This also
  // excudes the focus loss-gain on the new tab page from being counted.
  if (this.charactersCommitted_ > 0) {
    this.recordValue('InputMethod.VirtualKeyboard.CharactersCommitted',
          this.charactersCommitted_, 16384, 50);
    // TODO: Add WPM metrics.
  }
  this.charactersCommitted_ = 0;
  this.lastCommitLength_ = 0;
};


/**
 * Records the metrics for each commit.
 *
 * @param {string} source .
 * @param {string} target .
 * @param {number} targetIndex The target index.
 * @param {number} triggerType The trigger type:
 *     0: BySpace; 1: ByReset; 2: ByCandidate; 3: BySymbolOrNumber;
 *     4: ByDoubleSpaceToPeriod; 5: ByRevert; 6: ByVoice.
 */
Statistics.prototype.recordCommit = function(
    source, target, targetIndex, triggerType) {
  if (!this.inputMethodId_) {
    return;
  }
  var length = target.length;
  // Increment to include space.
  if (triggerType == 0) {
    length++;
  } else if (triggerType == 5) {
    length -= this.lastCommitLength_;
  }
  this.lastCommitLength_ = length;
  this.charactersCommitted_ += length;

  var CommitTypes = Statistics.CommitTypes;
  var commitType = -1;
  if (targetIndex == 0 && source == target) {
    commitType = CommitTypes.X_X0;
  } else if (targetIndex == 0 && source != target) {
    commitType = CommitTypes.X_Y0;
  } else if (targetIndex > 0 && source == target) {
    commitType = CommitTypes.X_X1;
  } else if (targetIndex > 0 && source != target) {
    commitType = CommitTypes.X_Y1;
  } else if (!source && this.getTargetType_(source, target) == 3) {
    commitType = CommitTypes.PREDICTION;
  } else if (triggerType == 5) {
    commitType = CommitTypes.REVERT;
  } else if (triggerType == 6) {
    commitType = CommitTypes.VOICE;
  }
  if (commitType < 0) {
    return;
  }

  // For latin transliteration, record the logs under the name with 'Pk' which
  // means Physical Keyboard.
  var name = this.isPhysicalKeyboard_ ?
      'InputMethod.PkCommit.' : 'InputMethod.Commit.';

  var self = this;
  var record = function(suffix) {
    self.recordEnum(name + 'Index' + suffix, targetIndex + 1, 20);
    self.recordEnum(name + 'Type' + suffix, commitType, CommitTypes.MAX);
  };

  record('');

  if (/^pinyin/.test(this.inputMethodId_)) {
    record('.Pinyin');
  } else if (/^xkb:us/.test(this.inputMethodId_)) {
    record('.US');
    record('.US.AC' + this.autoCorrectLevel_);
  } else if (/^xkb:fr/.test(this.inputMethodId_)) {
    record('.FR');
    record('.FR.AC' + this.autoCorrectLevel_);
  }
};


/**
 * Records the latency value for stats.
 *
 * @param {string} name .
 * @param {number} timeInMs .
 */
Statistics.prototype.recordLatency = function(
    name, timeInMs) {
  this.recordValue(name, timeInMs, 1000, 50);
};


/**
 * Gets the layout type for stats.
 *
 * @param {string} layoutCode .
 * @param {boolean} isA11yMode .
 * @return {Statistics.LayoutTypes}
 */
Statistics.prototype.getLayoutType = function(layoutCode, isA11yMode) {
  var LayoutTypes = Statistics.LayoutTypes;
  var layoutType = LayoutTypes.MAX;
  if (isA11yMode) {
    layoutType = LayoutTypes.A11Y;
  } else if (/compact/.test(layoutCode)) {
    if (/symbol/.test(layoutCode)) {
      layoutType = LayoutTypes.COMPACT_SYMBOL;
    } else if (/more/.test(layoutCode)) {
      layoutType = LayoutTypes.COMPACT_MORE;
    } else {
      layoutType = LayoutTypes.COMPACT;
    }
  } else if (/^hwt/.test(layoutCode)) {
    layoutType = LayoutTypes.HANDWRITING;
  } else if (/^emoji/.test(layoutCode)) {
    layoutType = LayoutTypes.EMOJI;
  }
  return layoutType;
};


/**
 * Records the layout usage.
 *
 * @param {string} layoutCode The layout code to be switched to.
 * @param {boolean} isA11yMode .
 */
Statistics.prototype.recordLayout = function(
    layoutCode, isA11yMode) {
  this.recordEnum('InputMethod.VirtualKeyboard.Layout',
      this.getLayoutType(layoutCode, isA11yMode), Statistics.LayoutTypes.MAX);
};


/**
 * Records the layout switching action.
 */
Statistics.prototype.recordLayoutSwitch = function() {
  this.recordValue('InputMethod.VirtualKeyboard.LayoutSwitch', 1, 1, 1);
};


/**
 * Records enum value.
 *
 * @param {string} name .
 * @param {number} enumVal .
 * @param {number} enumCount .
 */
Statistics.prototype.recordEnum = function(
    name, enumVal, enumCount) {
  if (chrome.metricsPrivate && chrome.metricsPrivate.recordValue) {
    chrome.metricsPrivate.recordValue({
      'metricName': name,
      'type': 'histogram-linear',
      'min': 0,
      'max': enumCount - 1,
      'buckets': enumCount
    }, enumVal);
  }
};


/**
 * Records count value.
 *
 * @param {string} name .
 * @param {number} count .
 * @param {number} max .
 * @param {number} bucketCount .
 */
Statistics.prototype.recordValue = function(
    name, count, max, bucketCount) {
  if (chrome.metricsPrivate && chrome.metricsPrivate.recordValue) {
    chrome.metricsPrivate.recordValue({
      'metricName': name,
      'type': 'histogram-log',
      'min': 0,
      'max': max,
      'buckets': bucketCount
    }, count);
  }
};


/**
 * Records a key down.
 */
Statistics.prototype.recordCharacterKey = function() {
  this.charactersBetweenBackspaces_++;
};


/**
 * Records a backspace.
 */
Statistics.prototype.recordBackspace = function() {
  // Ignore multiple backspaces typed in succession.
  if (this.charactersBetweenBackspaces_ > 0) {
    this.recordValue(
        'InputMethod.VirtualKeyboard.CharactersBetweenBackspaces',
        this.charactersBetweenBackspaces_, 4096, 50);
  }
  this.charactersBetweenBackspaces_ = 0;
};
});  // goog.scope
