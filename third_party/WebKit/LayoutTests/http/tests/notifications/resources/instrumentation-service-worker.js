importScripts('/resources/testharness-helpers.js');

// Allows a document to exercise the Notifications API within a service worker by sending commands.
var messagePort = null;

addEventListener('message', function(workerEvent) {
    messagePort = workerEvent.data;

    // Listen to incoming commands on the message port.
    messagePort.onmessage = function(event) {
        if (typeof event.data != 'object' || !event.data.command)
            return;

        switch (event.data.command) {
            case 'permission':
                messagePort.postMessage({ command: event.data.command,
                                          value: Notification.permission });
                break;

            case 'show':
                registration.showNotification(event.data.title, event.data.options).then(function() {
                    messagePort.postMessage({ command: event.data.command,
                                              success: true });
                }, function(error) {
                    messagePort.postMessage({ command: event.data.command,
                                              success: false,
                                              message: error.message });
                });
                break;

            default:
                messagePort.postMessage({ command: 'error', message: 'Invalid command: ' + event.data.command });
                break;
        }
    };

    // Notify the controller that the worker is now available.
    messagePort.postMessage('ready');
});

addEventListener('notificationclick', function(event) {
    // Copies the serializable attributes of the Notification instance on |event|.
    var notificationCopy = JSON.parse(stringifyDOMObject(event.notification));

    // Notifications containing "ACTION:CLOSE" in their message will be closed
    // immediately by the Service Worker.
    if (event.notification.body.indexOf('ACTION:CLOSE') != -1)
        event.notification.close();

    // Notifications containing "ACTION:OPENWINDOW" in their message will attempt
    // to open a new window for an example URL.
    if (event.notification.body.indexOf('ACTION:OPENWINDOW') != -1)
        event.waitUntil(clients.openWindow('https://example.com/'));

    messagePort.postMessage({ command: 'click',
                              notification: notificationCopy });
});
