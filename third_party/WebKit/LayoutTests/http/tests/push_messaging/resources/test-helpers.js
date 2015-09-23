"use strict";

// Subscribes and unsubscribes to push once so that the manifest details are stored in service
// worker storage. After this, subscribe can succeed from inside the service worker.
// This triggers an infobar unless a permission decision was previously set.
function subscribeAndUnsubscribePush(registration) {
    return new Promise(function(resolve, reject) {
        // 1. Call subscribe in document context. The manifest details are stored in the service
        // worker storage for later use in a service worker context where there is no manifest.
        registration.pushManager.subscribe()
            .then(function(subscription) {
                // 2. Call unsubscribe so we can subscribe again later inside a service worker.
                return subscription.unsubscribe();
            })
            .then(function(unsubscription_result) {
                resolve();
            })
            .catch(function(e) {
                reject(e);
            });
    });
}

// Runs |command| in the service worker connected to |port|. Returns a promise that will be resolved
// with the response data of the command.
function runCommandInServiceWorker(port, command) {
    return new Promise(function(resolve, reject) {
        port.addEventListener('message', function listener(event) {
            // To make this listener a oneshot, remove it the first time it runs.
            port.removeEventListener('message', listener);

            if (typeof event.data != 'object' || !event.data.command)
                assert_unreached('Invalid message from the service worker');

            assert_equals(event.data.command, command);
            if (event.data.success)
                resolve(event.data);
            else
                reject(new Error(event.data.errorMessage));
        });
        port.postMessage({command: command});
    });
}
