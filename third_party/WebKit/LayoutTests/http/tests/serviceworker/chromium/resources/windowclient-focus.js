// This helper will setup a small test framework that will use TESTS and run
// them sequentially and call self.postMessage('quit') when done.
// This helper also exposes |client|, |postMessage()|, |runNextTestOrQuit()|,
// |synthesizeNotificationClick()| and |initialize()|.
importScripts('sw-test-helpers.js');

// Clients nested inside the main client (|client|) used in this test.
var nestedClients = [];

// Override self.initialize() from sw-test-helpers.js
self.initialize = function() {
    return self.clients.getAll().then(function(clients) {
        clients.forEach(function(c) {
            // All clients are iframes but one of them embeds the other ones. We
            // want to use that one as the main |client|.
            // Its url ends with '.html' while the others ends with '.html?X'
            if (c.url.endsWith('.html'))
                client = c;
            else
                nestedClients.push(c);
        });
    });
};

function getNumberOfFocusedClients() {
  return self.clients.getAll().then(function(clients) {
    var focusedClients = 0;
    clients.forEach(function(c) {
      if (c.focused)
        ++focusedClients;
    });
    return focusedClients;
  });
}

var TESTS = [
    function testWithoutClick() {
        client.focus().catch(function(e) {
            self.postMessage('focus() can\'t focus a window without a user interaction');
            self.postMessage('focus() error is ' + e.name);
        }).then(runNextTestOrQuit);
    },

    function testFocusingFrame() {
        synthesizeNotificationClick().then(function(e) {
            client.focus().then(function(c) {
                self.postMessage('focus() succeeded');
                self.postMessage('focus() result: ' + c);
                self.postMessage(' url: ' + c.url);
                self.postMessage(' visibilityState: ' + c.visibilityState);
                self.postMessage(' focused: ' + c.focused);
                self.postMessage(' frameType: ' + c.frameType);
            }).then(getNumberOfFocusedClients)
            .then(function(count) {
                // There should be 1 focused client at this point.
                self.postMessage('focused clients: ' + count);
            }).then(runNextTestOrQuit);
        });
    },

    function testFocusNestedFrame() {
        synthesizeNotificationClick().then(function(e) {
            nestedClients[0].focus().then(function(c) {
                self.postMessage('focus() succeeded');
                self.postMessage('focus() result: ' + c);
                self.postMessage(' url: ' + c.url);
                self.postMessage(' visibilityState: ' + c.visibilityState);
                self.postMessage(' focused: ' + c.focused);
                self.postMessage(' frameType: ' + c.frameType);
            }).then(getNumberOfFocusedClients)
            .then(function(count) {
                // There should be 2 focused clients at this point.
                // The nested frame and its parent.
                self.postMessage('focused clients: ' + count);
            }).then(runNextTestOrQuit);
        });
    },

    function testFocusOtherNestedFrame() {
        synthesizeNotificationClick().then(function(e) {
            nestedClients[1].focus().then(function(c) {
                self.postMessage('focus() succeeded');
                self.postMessage('focus() result: ' + c);
                self.postMessage(' url: ' + c.url);
                self.postMessage(' visibilityState: ' + c.visibilityState);
                self.postMessage(' focused: ' + c.focused);
                self.postMessage(' frameType: ' + c.frameType);
            }).then(getNumberOfFocusedClients)
            .then(function(count) {
                // There should still be 2 focused clients at this point.
                // The nested frame and its parent.
                self.postMessage('focused clients: ' + count);
            }).then(runNextTestOrQuit);
        });
    },

    function testFocusOpenedWindow() {
        synthesizeNotificationClick().then(function(e) {
            return self.clients.openWindow('windowclient-focus.html?3');
        }).then(function(c) {
            synthesizeNotificationClick().then(function(e) {
                c.focus().then(function() {
                    self.postMessage('focus() succeeded');
                    self.postMessage('focus() result: ' + c);
                    self.postMessage(' url: ' + c.url);
                    self.postMessage(' visibilityState: ' + c.visibilityState);
                    self.postMessage(' focused: ' + c.focused);
                    self.postMessage(' frameType: ' + c.frameType);
                }).then(getNumberOfFocusedClients)
                .then(function(count) {
                    // There should be 1 focused clients at this point.
                    self.postMessage('focused clients: ' + count);
                }).then(runNextTestOrQuit);
            });
        });
    }
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
