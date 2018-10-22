// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function debug(message)
{
  var span = document.createElement("span");
  span.appendChild(document.createTextNode(message));
  span.appendChild(document.createElement("br"));
  document.getElementById('status').appendChild(span);
}

function done(message)
{
  if (document.location.hash == '#fail')
    return;
  if (message)
    debug('PASS: ' + message);
  else
    debug('PASS');
  document.location.hash = '#pass';
}

function fail(message)
{
  debug('FAILED: ' + message);
  document.location.hash = '#fail';
}

function getLog()
{
  return "" + document.getElementById('status').innerHTML;
}

function unexpectedUpgradeNeededCallback(e)
{
  fail('unexpectedUpgradeNeededCallback' +
       ' (oldVersion: ' + e.oldVersion + ' newVersion: ' + e.newVersion + ')');
}

function unexpectedAbortCallback(e)
{
  fail('unexpectedAbortCallback' +
      ' (' + e.target.error.name + ': ' + e.target.error.message + ')');
}

function unexpectedSuccessCallback()
{
  fail('unexpectedSuccessCallback');
}

function unexpectedCompleteCallback()
{
  fail('unexpectedCompleteCallback');
}

function unexpectedErrorCallback(e)
{
  fail('unexpectedErrorCallback' +
      ' (' + e.target.error.name + ': ' + e.target.error.message + ')');
}

function unexpectedBlockedCallback(e)
{
  fail('unexpectedBlockedCallback' +
       ' (oldVersion: ' + e.oldVersion + ' newVersion: ' + e.newVersion + ')');
}

function deleteAllObjectStores(db)
{
  objectStoreNames = db.objectStoreNames;
  for (var i = 0; i < objectStoreNames.length; ++i)
    db.deleteObjectStore(objectStoreNames[i]);
}

// The following functions are based on
// WebKit/LayoutTests/fast/js/resources/js-test-pre.js
// so that the tests will look similar to the existing layout tests.
function stringify(v)
{
  if (v === 0 && 1/v < 0)
    return "-0";
  else return "" + v;
}

function isResultCorrect(_actual, _expected)
{
  if (_expected === 0)
    return _actual === _expected && (1/_actual) === (1/_expected);
  if (_actual === _expected)
    return true;
  if (typeof(_expected) == "number" && isNaN(_expected))
    return typeof(_actual) == "number" && isNaN(_actual);
  if (Object.prototype.toString.call(_expected) ==
      Object.prototype.toString.call([]))
    return areArraysEqual(_actual, _expected);
  return false;
}

function shouldBe(_a, _b)
{
  if (typeof _a != "string" || typeof _b != "string")
    debug("WARN: shouldBe() expects string arguments");
  var exception;
  var _av;
  try {
    _av = eval(_a);
  } catch (e) {
    exception = e;
  }
  var _bv = eval(_b);

  if (exception)
    fail(_a + " should be " + _bv + ". Threw exception " + exception);
  else if (isResultCorrect(_av, _bv))
    debug(_a + " is " + _b);
  else if (typeof(_av) == typeof(_bv))
    fail(_a + " should be " + _bv + ". Was " + stringify(_av) + ".");
  else
    fail(_a + " should be " + _bv + " (of type " + typeof _bv + "). " +
         "Was " + _av + " (of type " + typeof _av + ").");
}

function shouldBeTrue(_a) { shouldBe(_a, "true"); }
function shouldBeFalse(_a) { shouldBe(_a, "false"); }
function shouldBeNaN(_a) { shouldBe(_a, "NaN"); }
function shouldBeNull(_a) { shouldBe(_a, "null"); }
function shouldBeEqualToString(a, b)
{
  var unevaledString = '"' + b.replace(/\\/g, "\\\\").replace(/"/g, "\"") + '"';
  shouldBe(a, unevaledString);
}

function indexedDBTest(upgradeCallback, optionalOpenCallback) {
  dbname = self.location.pathname.substring(
    1 + self.location.pathname.lastIndexOf("/"));
  var deleteRequest = indexedDB.deleteDatabase(dbname);
  deleteRequest.onerror = unexpectedErrorCallback;
  deleteRequest.onblocked = unexpectedBlockedCallback;
  deleteRequest.onsuccess = function() {
    var openRequest = indexedDB.open(dbname);
    openRequest.onerror = unexpectedErrorCallback;
    openRequest.onupgradeneeded = upgradeCallback;
    openRequest.onblocked = unexpectedBlockedCallback;
    if (optionalOpenCallback)
      openRequest.onsuccess = optionalOpenCallback;
  };
}

if (typeof String.prototype.startsWith !== 'function') {
  String.prototype.startsWith = function (str) {
    return this.indexOf(str) === 0;
  };
}
