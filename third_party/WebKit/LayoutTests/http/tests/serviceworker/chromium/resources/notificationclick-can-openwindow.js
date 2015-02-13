var client = null;

function initialize() {
    return self.clients.getAll().then(function(clients) {
        client = clients[0];
    });
}

function synthesizeNotificationClick() {
    var promise = new Promise(function(resolve) {
        var title = "fake notification";
        registration.showNotification(title).then(function() {
            client.postMessage({type: 'click', title: title});
        });

        var handler = function(e) {
            resolve(e);
            e.notification.close();
            self.removeEventListener('notificationclick', handler);
        };

        self.addEventListener('notificationclick', handler);
    });

    return promise;
}

var currentTest = -1;
var TESTS = [
    function testWithNoNotificationClick() {
        clients.openWindow('/foo.html').catch(function() {
            client.postMessage('openWindow() outside of a notificationclick event failed');
        }).then(runNextTestOrQuit);
    },

    function testInStackOutOfWaitUntil() {
        synthesizeNotificationClick().then(function() {
            clients.openWindow('/foo.html').then(function() {
                client.postMessage('openWindow() in notificationclick outside of waitUntil but in stack succeeded');
            }).then(runNextTestOrQuit);
        });
    },

    function testOutOfStackOutOfWaitUntil() {
        synthesizeNotificationClick().then(function() {
            self.clients.getAll().then(function() {
                clients.openWindow('/foo.html').catch(function() {
                    client.postMessage('openWindow() in notificationclick outside of waitUntil not in stack failed');
                }).then(runNextTestOrQuit);
            });
        });
    },

    function testInWaitUntilAsyncAndDoubleCall() {
        synthesizeNotificationClick().then(function(e) {
            e.waitUntil(self.clients.getAll().then(function() {
                return clients.openWindow('/foo.html').then(function() {
                    client.postMessage('openWindow() in notificationclick\'s waitUntil suceeded');
                }).then(runNextTestOrQuit);
            }));
        });
    },

    function testDoubleCallInWaitUntilAsync() {
        synthesizeNotificationClick().then(function(e) {
            e.waitUntil(self.clients.getAll().then(function() {
                return clients.openWindow('/foo.html').then(function() {
                    return clients.openWindow('/foo.html');
                }).catch(function() {
                    client.postMessage('openWindow() called twice failed');
                }).then(runNextTestOrQuit);
            }));
        });
    },


    function testWaitUntilTimeout() {
        var p = new Promise(function(resolve) {
            setTimeout(function() {
                resolve();
            }, 2000);
        });

        synthesizeNotificationClick().then(function(e) {
            e.waitUntil(p.then(function() {
                return clients.openWindow('/foo.html').catch(function() {
                    client.postMessage('openWindow() failed after timeout');
                }).then(runNextTestOrQuit);
            }));
        });
    },

    function testFocusWindowOpenWindowCombo() {
        synthesizeNotificationClick().then(function(e) {
            e.waitUntil(client.focus().then(function() {
                clients.openWindow().catch(function() {
                    client.postMessage('openWindow() failed because a window was focused before');
                }).then(runNextTestOrQuit);
            }));
        });
    },
];

function runNextTestOrQuit() {
    ++currentTest;
    if (currentTest >= TESTS.length) {
        client.postMessage('quit');
        return;
    }
    TESTS[currentTest]();
}

self.onmessage = function(e) {
    if (e.data == "start") {
        initialize().then(runNextTestOrQuit);
    } else {
        initialize().then(function() {
            client.postMessage('received unexpected message');
            client.postMessage('quit');
        });
    }
}
