// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

QUnit.module('error', {
  setup: function() {
  },
  teardown: function() {
  }
});

QUnit.test('error constructor 1', function() {
  var error = new remoting.Error(remoting.Error.Tag.HOST_OVERLOAD);
  QUnit.equal(error.getTag(), remoting.Error.Tag.HOST_OVERLOAD);
  QUnit.equal(error.toString(), remoting.Error.Tag.HOST_OVERLOAD);
});

QUnit.test('error constructor 2', function() {
  var error = new remoting.Error(
      remoting.Error.Tag.HOST_IS_OFFLINE,
      'detail');
  QUnit.equal(error.getTag(), remoting.Error.Tag.HOST_IS_OFFLINE);
  QUnit.ok(error.toString().indexOf(remoting.Error.Tag.HOST_IS_OFFLINE) != -1);
  QUnit.ok(error.toString().indexOf('detail') != -1);
});

QUnit.test('hasTag', function() {
  var error = new remoting.Error(remoting.Error.Tag.HOST_OVERLOAD);
  QUnit.ok(error.hasTag(remoting.Error.Tag.HOST_OVERLOAD));
  QUnit.ok(error.hasTag(
    remoting.Error.Tag.HOST_OVERLOAD,
    remoting.Error.Tag.HOST_IS_OFFLINE));
  QUnit.ok(!error.hasTag(remoting.Error.Tag.HOST_IS_OFFLINE));
});

QUnit.test('constructor methods', function() {
  QUnit.ok(remoting.Error.none().hasTag(remoting.Error.Tag.NONE));
  QUnit.ok(remoting.Error.unexpected().hasTag(remoting.Error.Tag.UNEXPECTED));
});

QUnit.test('isNone', function() {
  QUnit.ok(remoting.Error.none().isNone());
  QUnit.ok(!remoting.Error.unexpected().isNone());
  QUnit.ok(!new remoting.Error(remoting.Error.Tag.CANCELLED).isNone());
});


QUnit.test('fromHttpStatus', function() {
  QUnit.ok(remoting.Error.fromHttpStatus(200).isNone());
  QUnit.ok(remoting.Error.fromHttpStatus(201).isNone());
  QUnit.ok(!remoting.Error.fromHttpStatus(500).isNone());
  QUnit.equal(
      remoting.Error.fromHttpStatus(0).getTag(),
      remoting.Error.Tag.NETWORK_FAILURE);
  QUnit.equal(
      remoting.Error.fromHttpStatus(100).getTag(),
      remoting.Error.Tag.UNEXPECTED);
  QUnit.equal(
      remoting.Error.fromHttpStatus(200).getTag(),
      remoting.Error.Tag.NONE);
  QUnit.equal(
      remoting.Error.fromHttpStatus(201).getTag(),
      remoting.Error.Tag.NONE);
  QUnit.equal(
      remoting.Error.fromHttpStatus(400).getTag(),
      remoting.Error.Tag.AUTHENTICATION_FAILED);
  QUnit.equal(
      remoting.Error.fromHttpStatus(401).getTag(),
      remoting.Error.Tag.AUTHENTICATION_FAILED);
  QUnit.equal(
      remoting.Error.fromHttpStatus(402).getTag(),
      remoting.Error.Tag.UNEXPECTED);
  QUnit.equal(
      remoting.Error.fromHttpStatus(403).getTag(),
      remoting.Error.Tag.NOT_AUTHORIZED);
  QUnit.equal(
      remoting.Error.fromHttpStatus(404).getTag(),
      remoting.Error.Tag.NOT_FOUND);
  QUnit.equal(
      remoting.Error.fromHttpStatus(500).getTag(),
      remoting.Error.Tag.SERVICE_UNAVAILABLE);
  QUnit.equal(
      remoting.Error.fromHttpStatus(501).getTag(),
      remoting.Error.Tag.SERVICE_UNAVAILABLE);
  QUnit.equal(
      remoting.Error.fromHttpStatus(600).getTag(),
      remoting.Error.Tag.UNEXPECTED);
});

QUnit.test('handler 1', function() {
  /** @type {!remoting.Error} */
  var reportedError;
  var onError = function(/** !remoting.Error */ arg) {
    reportedError = arg;
  };
  remoting.Error.handler(onError)('not a real tag');
  QUnit.ok(reportedError instanceof remoting.Error);
  QUnit.equal(reportedError.getTag(), remoting.Error.Tag.UNEXPECTED);
});


QUnit.test('handler 2', function() {
  /** @type {!remoting.Error} */
  var reportedError;
  var onError = function(/** !remoting.Error */ arg) {
    reportedError = arg;
  };
  var origError = new remoting.Error(remoting.Error.Tag.HOST_IS_OFFLINE);
  remoting.Error.handler(onError)(origError);
  QUnit.equal(reportedError, origError);
});

})();
