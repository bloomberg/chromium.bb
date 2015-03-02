// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// How to create sinon.stubs that work with jscompile.
//
// To create the stub:
//   sinon.$setupStub(<object>, <function-name>)
//
// To access the stub in unittests:
//   <object>.<function-name>.$testStub.<sinon-test>
//
// For example:
//    sinon.$setupStub(chrome.socket, 'create');
//    chrome.socket.create.$testStub.restore();
//
// For jscompile to analyze these corectly, you'll also need to add an entry
// in this file for any object you stub out this way. For example:
//    chrome.socket.create.$testStub = new sinon.TestStub();

base.debug.assert.$testStub = new sinon.TestStub();
base.isAppsV2.$testStub = new sinon.TestStub();

chrome.i18n.getMessage.$testStub = new sinon.TestStub();

chrome.socket.connect.$testStub = new sinon.TestStub();
chrome.socket.create.$testStub = new sinon.TestStub();
chrome.socket.destroy.$testStub = new sinon.TestStub();
chrome.socket.read.$testStub = new sinon.TestStub();
chrome.socket.secure.$testStub = new sinon.TestStub();
chrome.socket.write.$testStub = new sinon.TestStub();
