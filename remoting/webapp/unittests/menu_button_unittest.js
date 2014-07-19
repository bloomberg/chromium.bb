// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

var menuButton = null;
var menuShown = false;

module('MenuButton', {
  setup: function() {
    var fixture = document.getElementById('qunit-fixture');
    fixture.innerHTML =
        '<span class="menu-button" id="menu-button-container">' +
          '<button class="menu-button-activator">Click me</button>' +
          '<ul>' +
            '<li id="menu-option-1">Option 1</li>' +
          '</ul>' +
        '</span>';
    menuButton = new remoting.MenuButton(
        document.getElementById('menu-button-container'),
        function() { menuShown = true; });
    menuShown = false;
  },
  teardown: function() {
  }
});

test('should display on click', function() {
  var menu = menuButton.menu();
  ok(menu.offsetWidth == 0 && menu.offsetHeight == 0);
  menuButton.button().click();
  ok(menu.offsetWidth != 0 && menu.offsetHeight != 0);
});

test('should dismiss when <body> is clicked', function() {
  var menu = menuButton.menu();
  menuButton.button().click();
  document.body.click();
  ok(menu.offsetWidth == 0 && menu.offsetHeight == 0);
});

test('should dismiss when button is clicked', function() {
  var menu = menuButton.menu();
  menuButton.button().click();
  menuButton.button().click();
  ok(menu.offsetWidth == 0 && menu.offsetHeight == 0);
});

test('should dismiss when menu item is clicked', function() {
  var menu = menuButton.menu();
  menuButton.button().click();
  var element = document.getElementById('menu-option-1');
  element.click();
  ok(menu.offsetWidth == 0 && menu.offsetHeight == 0);
});

test('should invoke callback', function() {
  ok(!menuShown);
  menuButton.button().click();
  ok(menuShown);
});

test('select method should set/unset background image', function() {
  var element = document.getElementById('menu-option-1');
  var style = window.getComputedStyle(element);
  ok(style.backgroundImage == 'none');
  remoting.MenuButton.select(element, true);
  style = window.getComputedStyle(element);
  ok(style.backgroundImage != 'none');
  remoting.MenuButton.select(element, false);
  style = window.getComputedStyle(element);
  ok(style.backgroundImage == 'none');
});

}());