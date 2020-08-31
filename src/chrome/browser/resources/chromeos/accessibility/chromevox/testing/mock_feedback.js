// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file contains the |MockFeedback| class which is
 * a combined mock class for speech, braille, and earcon feedback.  A
 * test that uses this class may add expectations for speech
 * utterances, braille display content to be output, and earcons
 * played (by name).  The |install| method sets appropriate mock
 * classes as the |ChromeVox.tts|, |ChromeVox.braille| and
 * |ChromeVox.earcons| objects, respectively.  Output sent to
 * those objects will then be collected in an internal queue.
 *
 * Expectations can be added using the |expectSpeech|,
 * |expectBraille|, and |expectEarcon| methods.  These methods take
 * either strings or regular expressions to match against.  Strings
 * must match a full utterance (or display content) exactly, while a
 * regular expression must match a substring (use anchor operators if
 * needed).
 *
 * Function calls may be inserted in the stream of expectations using the
 * |call| method.  Such callbacks are called after all preceding expectations
 * have been met, and before any further expectations are matched.  Callbacks
 * are called in the order they were added to the mock.
 *
 * The |replay| method starts processing any pending utterances,
 * braille display content, and earcons and will try to match
 * expectations as new feedback enters the queue asynchronously.  When
 * all expectations have been met and callbacks called, the finish
 * callback, if any was provided to the constructor, is called.
 *
 * This mock class is lean, meaning that feedback that doesn't match
 * any expectations is silently ignored.
 *
 * NOTE: for asynchronous tests, the processing will never finish if there
 * are unmet expectations.  To help debugging in such situations, the mock
 * will output its pending state if there are pending expectations and no
 * output is received within a few seconds.
 *
 * See mock_feedback_test.js for example usage of this class.
 */

/**
 * Combined mock class for braille and speech output.
 */
MockFeedback = class {
  /**
   * @param {function=} opt_finishedCallback Called when all expectations have
   *     been met.
   */
  constructor(opt_finishedCallback) {
    /**
     * @type {function}
     * @private
     */
    this.finishedCallback_ = opt_finishedCallback || null;
    /**
     * True when |replay| has been called and actions are being replayed.
     * @type {boolean}
     * @private
     */
    this.replaying_ = false;
    /**
     * True when inside the |process| function to prevent nested calls.
     * @type {boolean}
     * @private
     */
    this.inProcess_ = false;
    /**
     * Pending expectations and callbacks.
     * @type {Array<{perform(): boolean, toString(): string}>}
     * @private
     */
    this.pendingActions_ = [];
    /**
     * Pending speech utterances.
     * @type {Array<{text: string, callback: (function|undefined)}>}
     * @private
     */
    this.pendingUtterances_ = [];
    /**
     * Pending braille output.
     * @type {Array<{text: string, callback: (function|undefined)}>}
     * @private
     */
    this.pendingBraille_ = [];
    /**
     * Pending earcons.
     * @type {Array<{text: string, callback: (function|undefined)}>}
     * @private
     */
    this.pendingEarcons_ = [];
    /**
     * Handle for the timeout set for debug logging.
     * @type {number}
     * @private
     */
    this.logTimeoutId_ = 0;
    /**
     * @type {NavBraille}
     * @private
     */
    this.lastMatchedBraille_ = null;
  }

  /**
   * Install mock objects as |ChromeVox.tts| and |ChromeVox.braille|
   * to collect feedback.
   */
  install() {
    assertFalse(this.replaying_);

    const MockTts = function() {};
    MockTts.prototype = {
      __proto__: TtsInterface.prototype,
      speak: this.addUtterance_.bind(this)
    };

    ChromeVox.tts = new MockTts();

    const MockBraille = function() {};
    MockBraille.prototype = {
      __proto__: BrailleInterface.prototype,
      write: this.addBraille_.bind(this)
    };

    ChromeVox.braille = new MockBraille();

    const MockEarcons = function() {};
    MockEarcons.prototype = {
      __proto__: AbstractEarcons.prototype,
      playEarcon: this.addEarcon_.bind(this)
    };

    // ChromeVox.earcons is a getter that switches between Classic and
    // Next; replace it with MockEarcons.
    delete ChromeVox.earcons;
    ChromeVox.earcons = new MockEarcons();
  }

  /**
   * Returns true if |utterance| is in |pendingUtterances_|.
   * @param {string} utterance
   * @return {boolean}
   */
  utteranceInQueue(utterance) {
    for (const pendingUtterance of this.pendingUtterances_) {
      if (pendingUtterance.text === utterance) {
        return true;
      }
    }
    return false;
  }

  /**
   * Adds an expectation for one or more spoken utterances.
   * @param {...(string|RegExp)} var_args One or more utterance to add as
   *     expectations.
   * @return {MockFeedback} |this| for chaining
   */
  expectSpeech() {
    assertFalse(this.replaying_);
    Array.prototype.forEach.call(arguments, function(text) {
      this.pendingActions_.push({
        perform: function() {
          return !!MockFeedback.matchAndConsume_(
              text, {}, this.pendingUtterances_);
        }.bind(this),
        toString() {
          return 'Speak \'' + text + '\'';
        }
      });
    }.bind(this));
    return this;
  }

  /**
   * Adds an expectation for one spoken utterance that will be enqueued
   *     with a given queue mode.
   * @param {string|RegExp} text One utterance expectation.
   * @param {QueueMode} queueMode The expected queue mode.
   * @return {MockFeedback} |this| for chaining
   */
  expectSpeechWithQueueMode(text, queueMode) {
    return this.expectSpeechWithProperties.apply(this, [{queueMode}, text]);
  }

  /**
   * Adds an expectation for one spoken utterance that will be queued.
   * @param {string|RegExp} text One utterance expectation.
   * @return {MockFeedback} |this| for chaining
   */
  expectQueuedSpeech(text) {
    return this.expectSpeechWithQueueMode(text, QueueMode.QUEUE);
  }

  /**
   * Adds an expectation for one spoken utterance that will be flushed.
   * @param {string|RegExp} text One utterance expectation.
   * @return {MockFeedback} |this| for chaining
   */
  expectFlushingSpeech(text) {
    return this.expectSpeechWithQueueMode(text, QueueMode.FLUSH);
  }

  /**
   * Adds an expectation for one spoken utterance that will be queued
   *     with the category flush mode.
   * @param {string|RegExp} text One utterance expectation.
   * @return {MockFeedback} |this| for chaining
   */
  expectCategoryFlushSpeech(text) {
    return this.expectSpeechWithQueueMode(text, QueueMode.CATEGORY_FLUSH);
  }

  /**
   * Adds expectations for spoken utterances with specified language.
   * @param {string} language The expected output language for utterances.
   * @param {...(string)} rest One or more utterances to add as expectations.
   * @return {MockFeedback} |this| for chaining
   */
  expectSpeechWithLocale(language, ...rest) {
    return this.expectSpeechWithProperties.apply(
        this, [{lang: language}].concat(rest));
  }

  /**
   * Adds expectations for spoken utterances with properties.
   * @param {!Object} expectedProps
   * @param {...(string)} rest One or more utterances to add as expectations.
   * @return {MockFeedback} |this| for chaining
   */
  expectSpeechWithProperties(expectedProps, ...rest) {
    assertFalse(this.replaying_);
    Array.prototype.forEach.call(rest, function(text) {
      this.pendingActions_.push({
        perform: function() {
          return !!MockFeedback.matchAndConsume_(
              text, expectedProps, this.pendingUtterances_);
        }.bind(this),
        toString() {
          return 'Speak \'' + text + '\' with props ' +
              JSON.stringify(expectedProps);
        }
      });
    }.bind(this));
    return this;
  }

  /**
   * Adds an expectation that the next spoken utterances do *not* match
   * the given arguments.
   *
   * This is only guaranteed to work for the immediately following utterance.
   * If you use it to check an utterance other than the immediately following
   * one the results may be flaky.
   *
   * @param {...(string|RegExp)} var_args One or more utterance to add as
   *     negative expectations.
   * @return {MockFeedback} |this| for chaining
   */
  expectNextSpeechUtteranceIsNot() {
    assertFalse(this.replaying_);
    Array.prototype.forEach.call(arguments, function(text) {
      this.pendingActions_.push({
        perform: function() {
          if (this.pendingUtterances_.length == 0) {
            return false;
          }
          if (MockFeedback.matchAndConsume_(
                  text, {}, this.pendingUtterances_)) {
            throw new Error('Got disallowed utterance "' + text + '".');
          }
          return true;
        }.bind(this),
        toString() {
          return 'Do not speak \'' + text + '\'';
        }
      });
    }.bind(this));
    return this;
  }

  /**
   * Adds an expectation for braille output.
   * @param {string|RegExp} text
   * @param {Object=} opt_props Additional properties to match in the
   *     |NavBraille|
   * @return {MockFeedback} |this| for chaining
   */
  expectBraille(text, opt_props) {
    assertFalse(this.replaying_);
    const props = opt_props || {};
    this.pendingActions_.push({
      perform: function() {
        const match =
            MockFeedback.matchAndConsume_(text, props, this.pendingBraille_);
        if (match) {
          this.lastMatchedBraille_ = match;
        }
        return !!match;
      }.bind(this),
      toString() {
        return 'Braille \'' + text + '\' ' + JSON.stringify(props);
      }
    });
    return this;
  }

  /**
   * Adds an expectation for a played earcon.
   * @param {string} earconName The name of the earcon.
   * @return {MockFeedback} |this| for chaining
   */
  expectEarcon(earconName, opt_props) {
    assertFalse(this.replaying_);
    this.pendingActions_.push({
      perform: function() {
        const match =
            MockFeedback.matchAndConsume_(earconName, {}, this.pendingEarcons_);
        return !!match;
      }.bind(this),
      toString() {
        return 'Earcon \'' + earconName + '\'';
      }
    });
    return this;
  }

  /**
   * Arranges for a callback to be invoked when all expectations that were
   * added before this call have been met.  Callbacks are called in the
   * order they are added.
   * @param {Function} callback
   * @return {MockFeedback} |this| for chaining
   */
  call(callback) {
    assertFalse(this.replaying_);
    this.pendingActions_.push({
      perform() {
        callback();
        return true;
      },
      toString() {
        return 'Callback';
      }
    });
    return this;
  }

  /**
   * Clears all pending output. Useful in cases where previous output might
   * overlap with future expectations.
   * @return {MockFeedback} |this| for chaining
   */
  clearPendingOutput() {
    this.call(function() {
      this.pendingUtterances_.length = 0;
      this.pendingBraille_.length = 0;
      this.pendingEarcons_.length = 0;
    }.bind(this));

    return this;
  }

  /**
   * Processes any feedback that has been received so far and tries to
   * satisfy the registered expectations.  Any feedback that is received
   * after this call (via the installed mock objects) is processed immediately.
   * When all expectations are satisfied and registered callbacks called,
   * the finish callbcak, if any, is called.
   * This function may only be called once.
   */
  replay() {
    assertFalse(this.replaying_);
    this.replaying_ = true;
    this.process_();
  }

  /**
   * Returns the |NavBraille| that matched an expectation.  This is
   * intended to be used by a callback to invoke braille commands that
   * depend on display contents.
   * @type {NavBraille}
   */
  get lastMatchedBraille() {
    assertTrue(this.replaying_);
    return this.lastMatchedBraille_;
  }

  /**
   * @param {string} textString
   * @param {QueueMode} queueMode
   * @param {Object=} properties
   * @private
   */
  addUtterance_(textString, queueMode, properties) {
    let callback;
    if (properties && (properties.startCallback || properties.endCallback)) {
      const startCallback = properties.startCallback;
      const endCallback = properties.endCallback;
      callback = function() {
        startCallback && startCallback();
        endCallback && endCallback();
      };
    }
    // Make a copy of all properties in a single object to be used in
    // matchAndConsume.
    const allProperties = {text: textString, queueMode, properties, callback};
    this.pendingUtterances_.push(allProperties);
    this.process_();
  }

  /** @private */
  addBraille_(navBraille) {
    this.pendingBraille_.push(navBraille);
    this.process_();
  }

  /** @private */
  addEarcon_(earconName) {
    this.pendingEarcons_.push({text: earconName});
    this.process_();
  }

  /*** @private */
  process_() {
    if (!this.replaying_ || this.inProcess_) {
      return;
    }
    try {
      this.inProcess_ = true;
      while (this.pendingActions_.length > 0) {
        const action = this.pendingActions_[0];
        if (action.perform()) {
          this.pendingActions_.shift();
          if (this.logTimeoutId_) {
            window.clearTimeout(this.logTimeoutId_);
            this.logTimeoutId_ = 0;
          }
        } else {
          break;
        }
      }
      if (this.pendingActions_.length == 0) {
        if (this.finishedCallback_) {
          this.finishedCallback_();
          this.finishedCallback_ = null;
        }
      } else {
        // If there are pending actions and no matching feedback for a few
        // seconds, log the pending state to ease debugging.
        if (!this.logTimeoutId_) {
          this.logTimeoutId_ =
              window.setTimeout(this.logPendingState_.bind(this), 2000);
        }
      }
    } finally {
      this.inProcess_ = false;
    }
  }

  /** @private */
  logPendingState_() {
    if (this.pendingActions_.length > 0) {
      console.log('Still waiting for ' + this.pendingActions_[0].toString());
    }
    function logPending(desc, list) {
      if (list.length > 0) {
        console.log(
            'Pending ' + desc + ':\n  ' +
            list.map(function(i) {
                  let ret = '\'' + i.text + '\'';
                  if ('properties' in i) {
                    ret += ' properties=' + JSON.stringify(i.properties);
                  }
                  if ('startIndex' in i) {
                    ret += ' startIndex=' + i.startIndex;
                  }
                  if ('endIndex' in i) {
                    ret += ' endIndex=' + i.endIndex;
                  }
                  return ret;
                })
                .join('\n  ') +
            '\n  ');
      }
    }
    logPending('speech utterances', this.pendingUtterances_);
    logPending('braille', this.pendingBraille_);
    logPending('earcons', this.pendingEarcons_);
    this.logTimeoutId_ = 0;
    }

    /**
     * @param {string} text
     * @param {Object} props
     * @param {Array<{text: (string|RegExp), callback: (function|undefined)}>}
     *     pending
     * @return {Object}
     * @private
     */
    static matchAndConsume_(text, props, pending) {
      let i, candidate;
      for (i = 0; candidate = pending[i]; ++i) {
        let candidateText = candidate.text;
        if (typeof (candidateText) != 'string') {
          candidateText = candidateText.toString();
        }

        if (text === candidateText ||
            (text instanceof RegExp && text.test(candidateText)) ||
            (typeof (text) == 'function' && text(candidate))) {
          let matched = true;
          for (const prop in props) {
            if (candidate[prop] !== props[prop] &&
                (!candidate.properties ||
                 candidate.properties[prop] != props[prop])) {
              matched = false;
              break;
            }
          }
          if (matched) {
            break;
          }
        }
      }
      if (candidate) {
        const consumed = pending.splice(0, i + 1);
        consumed.forEach(function(item) {
          if (item.callback) {
            item.callback();
          }
        });
      }
      return candidate;
    }
};
