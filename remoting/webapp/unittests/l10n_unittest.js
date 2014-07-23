// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

module('l10n', {
  setup: function() {
    sinon.stub(chrome.i18n, 'getMessage');
  },
  teardown: function() {
    chrome.i18n.getMessage.restore();
  }
});

test('getTranslationOrError(tag) should return tag on error', function() {
  var translation = l10n.getTranslationOrError('non_existent_tag');
  equal(translation, 'non_existent_tag');
});

test('localizeElementFromTag() should replace innerText by default',
  function() {
    var element = document.createElement('div');
    chrome.i18n.getMessage.withArgs('tag').returns('<b>Hello World</b>');

    l10n.localizeElementFromTag(element, 'tag');

    equal(element.innerHTML, '&lt;b&gt;Hello World&lt;/b&gt;');
});

test('localizeElementFromTag() should replace innerHTML if flag is set',
  function() {
    var element = document.createElement('div');
    chrome.i18n.getMessage.withArgs('tag').returns('<b>Hello World</b>');

    l10n.localizeElementFromTag(element, 'tag', null, true);

    equal(element.innerHTML, '<b>Hello World</b>');
});

test(
  'localizeElement() should replace innerText using the "i18n-content" ' +
  'attribute as the tag',
  function() {
    var element = document.createElement('div');
    element.setAttribute('i18n-content', 'tag');
    chrome.i18n.getMessage.withArgs('tag').returns('<b>Hello World</b>');

    l10n.localizeElement(element);

    equal(element.innerHTML, '&lt;b&gt;Hello World&lt;/b&gt;');
});

test(
  'localize() should replace element title using the "i18n-title" ' +
  'attribute as the tag',
  function() {
    var fixture = document.getElementById('qunit-fixture');
    fixture.innerHTML = '<div class="target" i18n-title="tag"></div>';
    chrome.i18n.getMessage.withArgs('tag').returns('localized title');

    l10n.localize();

    var target = document.querySelector('.target');
    equal(target.title, 'localized title');
});

test('localize() should support string substitutions', function() {
  var fixture = document.getElementById('qunit-fixture');
  fixture.innerHTML =
  '<div class="target" ' +
      'i18n-content="tag" ' +
      'i18n-value-1="param1" ' +
      'i18n-value-2="param2">' +
  '</div>';

  chrome.i18n.getMessage.withArgs('tag', ['param1', 'param2'])
      .returns('localized');

  l10n.localize();

  var target = document.querySelector('.target');
  equal(target.innerText, 'localized');
});

test('localize() should support tag substitutions', function() {
  var fixture = document.getElementById('qunit-fixture');
  fixture.innerHTML =
      '<div class="target" i18n-content="tag"' +
      ' i18n-value-name-1="tag1" i18n-value-name-2="tag2"></div>';

  var getMessage = chrome.i18n.getMessage;
  getMessage.withArgs('tag1').returns('param1');
  getMessage.withArgs('tag2').returns('param2');
  getMessage.withArgs('tag', ['param1', 'param2']).returns('localized');

  l10n.localize();

  var target = document.querySelector('.target');
  equal(target.innerText, 'localized');
});

})();
