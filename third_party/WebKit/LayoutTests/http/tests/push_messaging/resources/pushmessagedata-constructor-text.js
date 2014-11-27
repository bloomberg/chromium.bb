importScripts('../../serviceworker/resources/worker-testharness.js');
importScripts('/resources/testharness-helpers.js');

test(function() {
    assert_true('PushMessageData' in self);

}, 'PushMessageData is exposed on the Service Worker global scope.');

test(function() {
    var data = new PushMessageData('Hello, world!');
    assert_equals(data.text(), 'Hello, world!');

}, 'PushMessageData can be constructed with a string parameter.');

test(function() {
    var event = new PushEvent('PushEvent');
    assert_equals(event.data.text().length, 0);

}, 'PushMessageData is empty by default, given a PushEvent constructor.');

test(function() {
    var event = new PushEvent('PushEvent', {
        data: new PushMessageData('Hello, world!')
    });
    assert_equals(event.data.text(), 'Hello, world!');

}, 'Reading text from PushMessageData through the PushEvent constructor.');
