// This helper will setup a small test framework that will use TESTS and run
// them sequentially and call self.postMessage('quit') when done.
// This helper also exposes |client|, |postMessage()|, |runNextTestOrQuit()|,
// |synthesizeNotificationClick()| and |initialize()|.
importScripts('sw-test-helpers.js');

var TESTS = [
    function testWithNoNotificationClick() {
        clients.openWindow('/').catch(function(e) {
            self.postMessage('openWindow() can\'t open a window without a user interaction');
            self.postMessage('openWindow() error is ' + e.name);
        }).then(runNextTestOrQuit);
    },

    function testOpenCrossOriginWindow() {
        synthesizeNotificationClick().then(function(e) {
            clients.openWindow('https://test.com/').then(function(c) {
                self.postMessage('openWindow() can open cross origin windows');
                self.postMessage('openWindow() result: ' + c);
            }).then(runNextTestOrQuit);
        });
    },

    function testOpenNotControlledWindow() {
        synthesizeNotificationClick().then(function(e) {
            clients.openWindow('/').then(function(c) {
                self.postMessage('openWindow() can open not controlled windows');
                self.postMessage('openWindow() result: ' + c);
            }).then(runNextTestOrQuit);
        });
    },

    function testOpenControlledWindow() {
        synthesizeNotificationClick().then(function(e) {
            clients.openWindow('blank.html').then(function(c) {
                self.postMessage('openWindow() can open controlled windows');
                self.postMessage('openWindow() result: ' + c);
                self.postMessage(' url: ' + c.url);
                self.postMessage(' visibilityState: ' + c.visibilityState);
                self.postMessage(' focused: ' + c.focused);
                self.postMessage(' frameType: ' + c.frameType);
            }).then(runNextTestOrQuit);
        });
    },

    function testOpenAboutBlank() {
        synthesizeNotificationClick().then(function(e) {
            clients.openWindow('about:blank').then(function(c) {
                self.postMessage('openWindow() can open about:blank');
                self.postMessage('openWindow() result: ' + c);
            }).then(runNextTestOrQuit);
        });
    },

    function testOpenAboutCrash() {
        synthesizeNotificationClick().then(function(e) {
            clients.openWindow('about:crash').then(function(c) {
                self.postMessage('openWindow() can open about:crash');
                self.postMessage('openWindow() result: ' + c);
            }).then(runNextTestOrQuit);
        });
    },

    function testOpenViewSource() {
        synthesizeNotificationClick().then(function(e) {
            clients.openWindow('view-source://http://test.com').catch(function(c) {
                self.postMessage('openWindow() can not open view-source scheme');
            }).then(runNextTestOrQuit);
        });
    },

    function testOpenFileScheme() {
        synthesizeNotificationClick().then(function(e) {
            clients.openWindow('file:///').catch(function(error) {
                self.postMessage('openWindow() can not open file scheme');
                self.postMessage('openWindow() error is: ' + error.name);
            }).then(runNextTestOrQuit);
        });
    },
];

self.onmessage = function(e) {
    if (e.data == 'start') {
        initialize().then(runNextTestOrQuit);
    } else {
        initialize().then(function() {
            self.postMessage('received unexpected message');
            self.postMessage('quit');
        });
    }
};
