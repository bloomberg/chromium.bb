// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult("Tests that wild card pattern matching works.\n");

  checkPattern('*a*a*', 'asdfasdf');
  checkPattern('a*a*', 'asdfasdf');
  checkPattern('a*a', 'asdfasdf');
  checkPattern('a*a*f', 'asdfasdf');
  checkPattern('af', 'asdfasdf');
  checkPattern('*df', 'asdfasdf');
  checkPattern('*', 'asdfasdf');
  checkPattern('*', '');
  checkPattern('', '');
  checkPattern('', 'asdfasdf');
  checkPattern('a*c', 'ac');
  checkPattern('a**c', 'ac');
  checkPattern('a**c', 'abc');
  checkPattern('a*c', 'abc');
  checkPattern('*ac*', 'ac');
  checkPattern('ac', 'abc');
  checkPattern('a\\*c', 'a*c');
  checkPattern('a\\\\*c', 'a\\c');
  checkPattern('a\\*c', 'ac');
  checkPattern('a\\*c', 'a**c');
  checkPattern('a\\*\\\\*c', 'a*\\*c');
  checkPattern('a\\*\\\\\\*c', 'a*\\*c');
  checkPattern('a?c', 'a?c');
  checkPattern('a?c', 'acc');

  TestRunner.completeTest();

  function checkPattern(pattern, input) {
    TestRunner.addResult("Checking Pattern: " + pattern);
    TestRunner.addResult("Input: " + input);
    TestRunner.addResult("Result: " + SDK.RequestInterceptor.patternMatchesUrl(pattern, input));
    TestRunner.addResult("");
  }
})();
