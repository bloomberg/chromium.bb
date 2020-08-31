// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '../testing/chromevox_e2e_test_base.js', '../testing/assert_additions.js'
]);

// E2E tests for TtsBackground.

/**
 * Test fixture.
 */
ChromeVoxTtsBackgroundTest = class extends ChromeVoxE2ETest {};


SYNC_TEST_F('ChromeVoxTtsBackgroundTest', 'Preprocess', function() {
  const tts = new TtsBackground(false);
  const preprocess = tts.preprocess.bind(tts);

  // Punctuation.
  assertEquals('dot', preprocess('.'));
  assertEquals('x.', preprocess('x.'));
  assertEquals('.x', preprocess('.x'));
  assertEquals('space', preprocess(' '));
  assertEquals('', preprocess('  '));
  assertEquals('A', preprocess('a'));
  assertEquals('A', preprocess('A'));
  assertEquals('a.', preprocess('a.'));
  assertEquals('.a', preprocess('.a'));

  // Only summarize punctuation if there are three or more occurrences without
  // a space in between.
  assertEquals('10 equal signs', preprocess('=========='));
  assertEquals('bullet bullet bullet', preprocess('\u2022 \u2022\u2022'));
  assertEquals('3 bullets', preprocess('\u2022\u2022\u2022'));
  assertEquals('C plus plus', preprocess('C++'));
  assertEquals('C 3 plus signs', preprocess('C+++'));
  // There are some punctuation marks that we do not verbalize because they
  // result in an overly verbose experience (periods, commas, dashes, etc.).
  // This set of punctuation marks is defined in the |some_punctuation| regular
  // expression in TtsBackground.
  assertEquals('C--', preprocess('C--'));
  assertEquals('Hello world.', preprocess('Hello world.'));

  assertEquals('new line', preprocess('\n'));
  assertEquals('return', preprocess('\r'));
});

TEST_F('ChromeVoxTtsBackgroundTest', 'UpdateVoice', function() {
  const tts = new TtsBackground(false);
  const voices = [
    {lang: 'zh-CN', voiceName: 'Chinese'},
    {lang: 'zh-TW', voiceName: 'Chinese (Taiwan)'},
    {lang: 'es', voiceName: 'Spanish'},
    {lang: 'en-US', voiceName: 'U.S. English'}
  ];

  chrome.tts.getVoices = function(callback) {
    callback(voices);
  };

  // Asks this test to process the next task immediately.
  const flushNextTask = function() {
    const task = tasks.shift();
    if (!task) {
      return;
    }

    if (task.setup) {
      task.setup();
    }
    tts.updateVoice_(task.testVoice, this.newCallback(function(actualVoice) {
      assertEquals(task.expectedVoice, actualVoice);
      flushNextTask();
    }));
  }.bind(this);

  assertTrue(!tts.currentVoice);

  const tasks = [
    {testVoice: '', expectedVoice: constants.SYSTEM_VOICE},

    {
      setup() {
        voices[3].lang = 'en';
      },
      testVoice: 'U.S. English',
      expectedVoice: 'U.S. English'
    },

    {
      setup() {
        voices[3].lang = 'fr-FR';
        voices[3].voiceName = 'French';
      },
      testVoice: '',
      expectedVoice: constants.SYSTEM_VOICE
    },

    {testVoice: 'French', expectedVoice: 'French'},

    {testVoice: 'NotFound', expectedVoice: constants.SYSTEM_VOICE},
  ];

  flushNextTask();
});

// This test only works if Google tts is installed. Run it locally.
TEST_F(
    'ChromeVoxTtsBackgroundTest', 'DISABLED_EmptyStringCallsCallbacks',
    function() {
      const tts = new TtsBackground(false);
      let startCalls = 0, endCalls = 0;
      assertCallsCallbacks = function(text, speakCalls) {
        tts.speak(text, QueueMode.QUEUE, {
          startCallback() {
            ++startCalls;
          },
          endCallback: this.newCallback(function() {
            ++endCalls;
            assertEquals(speakCalls, endCalls);
            assertEquals(endCalls, startCalls);
          })
        });
      }.bind(this);

      assertCallsCallbacks('', 1);
      assertCallsCallbacks('  ', 2);
      assertCallsCallbacks(' \u00a0 ', 3);
    });

SYNC_TEST_F(
    'ChromeVoxTtsBackgroundTest', 'CapitalizeSingleLettersAfterNumbers',
    function() {
      const tts = new TtsBackground(false);
      const preprocess = tts.preprocess.bind(tts);

      // Capitalize single letters if they appear directly after a number.
      assertEquals(
          'Click to join the 5G network',
          preprocess('Click to join the 5g network'));
      assertEquals(
          'I ran a 100M sprint in 10 S',
          preprocess('I ran a 100m sprint in 10 s'));
      // Do not capitalize the letter "a".
      assertEquals(
          'Please do the shopping at 3 a thing came up at work',
          preprocess('Please do the shopping at 3 a thing came up at work'));
    });

SYNC_TEST_F('ChromeVoxTtsBackgroundTest', 'AnnounceCapitalLetters', function() {
  const tts = new TtsBackground(false);
  const preprocess = tts.preprocess.bind(tts);

  assertEquals('A', preprocess('A'));

  // Only announce capital for solo capital letters.
  localStorage['capitalStrategy'] = 'announceCapitals';
  assertEquals('Cap A', preprocess('A'));
  assertEquals('Cap Z', preprocess('Z'));

  // Do not announce capital for the following inputs.
  assertEquals('BB', preprocess('BB'));
  assertEquals('A.', preprocess('A.'));
});

SYNC_TEST_F('ChromeVoxTtsBackgroundTest', 'NumberReadingStyle', function() {
  const tts = new TtsBackground();
  let lastSpokenTextString = '';
  tts.speakUsingQueue_ = function(utterance, _) {
    lastSpokenTextString = utterance.textString;
  };

  // Check the default preference.
  assertEquals('asWords', localStorage['numberReadingStyle']);

  tts.speak('100');
  assertEquals('100', lastSpokenTextString);

  tts.speak('An unanswered call lasts for 30 seconds.');
  assertEquals(
      'An unanswered call lasts for 30 seconds.', lastSpokenTextString);

  localStorage['numberReadingStyle'] = 'asDigits';
  tts.speak('100');
  assertEquals('1 0 0', lastSpokenTextString);

  tts.speak('An unanswered call lasts for 30 seconds.');
  assertEquals(
      'An unanswered call lasts for 3 0 seconds.', lastSpokenTextString);
});

SYNC_TEST_F('ChromeVoxTtsBackgroundTest', 'SplitLongText', function() {
  const tts = new TtsBackground();
  const spokenTextStrings = [];
  tts.speakUsingQueue_ = function(utterance, _) {
    spokenTextStrings.push(utterance.textString);
  };

  // There are three new lines, but only the first chunk exceeds the max
  // character count.
  const text = 'a'.repeat(constants.OBJECT_MAX_CHARCOUNT) + '\n' +
      'a'.repeat(10) + '\n' +
      'a'.repeat(10);
  tts.speak(text);
  assertEquals(2, spokenTextStrings.length);
});

SYNC_TEST_F('ChromeVoxTtsBackgroundTest', 'SplitUntilSmall', function() {
  const split = TtsBackground.splitUntilSmall;

  // A single delimiter.
  constants.OBJECT_MAX_CHARCOUNT = 3;
  assertEqualsJSON(['12', '345', '789'], split('12345a789', 'a'));

  constants.OBJECT_MAX_CHARCOUNT = 4;
  assertEqualsJSON(['12', '345', '789'], split('12345a789', 'a'));

  constants.OBJECT_MAX_CHARCOUNT = 7;
  assertEqualsJSON(['12345', '789'], split('12345a789', 'a'));

  constants.OBJECT_MAX_CHARCOUNT = 10;
  assertEqualsJSON(['12345a789'], split('12345a789', 'a'));

  // Multiple delimiters.
  constants.OBJECT_MAX_CHARCOUNT = 3;
  assertEqualsJSON(['12', '34', '57', '89'], split('1234b57a89', 'ab'));

  constants.OBJECT_MAX_CHARCOUNT = 4;
  assertEqualsJSON(['1234', '57', '89'], split('1234b57a89', 'ab'));

  constants.OBJECT_MAX_CHARCOUNT = 5;
  assertEqualsJSON(['12345', '789'], split('12345b789a', 'ab'));
  assertEqualsJSON(['12345', '789a'], split('12345b789a', 'ba'));

  // No delimiters.
  constants.OBJECT_MAX_CHARCOUNT = 3;
  assertEqualsJSON(['12', '34', '57', '89'], split('12345789', ''));

  constants.OBJECT_MAX_CHARCOUNT = 4;
  assertEqualsJSON(['1234', '5789'], split('12345789', ''));

  // Some corner cases.
  assertEqualsJSON([], split('', ''));
  assertEqualsJSON(['a'], split('a', ''));
  assertEqualsJSON(['a'], split('a', 'a'));
  assertEqualsJSON(['a'], split('a', 'b'));
});

SYNC_TEST_F('ChromeVoxTtsBackgroundTest', 'Phonetics', function() {
  const tts = new TtsBackground(false);
  let spokenStrings = [];
  tts.speakUsingQueue_ = (utterance, ...rest) => {
    spokenStrings.push(utterance.textString);
  };

  // English.
  tts.speak('t', QueueMode.QUEUE, {lang: 'en-us', phoneticCharacters: true});
  assertTrue(spokenStrings.includes('T'));
  assertTrue(spokenStrings.includes('tango'));
  tts.speak('a', QueueMode.QUEUE, {lang: 'en-us', phoneticCharacters: true});
  assertTrue(spokenStrings.includes('A'));
  assertTrue(spokenStrings.includes('alpha'));
  spokenStrings = [];

  // German.
  tts.speak('t', QueueMode.QUEUE, {lang: 'de', phoneticCharacters: true});
  assertTrue(spokenStrings.includes('T'));
  assertTrue(spokenStrings.includes('Theodor'));
  tts.speak('a', QueueMode.QUEUE, {lang: 'de', phoneticCharacters: true});
  assertTrue(spokenStrings.includes('A'));
  assertTrue(spokenStrings.includes('Anton'));
  spokenStrings = [];

  // Japanese.
  tts.speak('t', QueueMode.QUEUE, {lang: 'ja', phoneticCharacters: true});
  assertTrue(spokenStrings.includes('T'));
  assertTrue(spokenStrings.includes('ティー タイム'));
  tts.speak('a', QueueMode.QUEUE, {lang: 'ja', phoneticCharacters: true});
  assertTrue(spokenStrings.includes('A'));
  assertTrue(spokenStrings.includes('エイ アニマル'));
  tts.speak('人', QueueMode.QUEUE, {lang: 'ja', phoneticCharacters: true});
  assertTrue(spokenStrings.includes('人'));
  assertTrue(spokenStrings.includes('ヒト，ニンゲン ノ ニン'));
  spokenStrings = [];

  // Error handling.
  tts.speak('t', QueueMode.QUEUE, {lang: 'qwerty', phoneticCharacters: true});
  assertTrue(spokenStrings.includes('T'));
  assertEquals(1, spokenStrings.length);
});
