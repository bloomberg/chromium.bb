// Sends a message to all clients when the notificationclick event gets received.
var messagePort = null;

addEventListener('message', function(event) {
    messagePort = event.data;
    messagePort.postMessage('ready');
});

addEventListener('notificationclick', function(event) {
    // FIXME: Include the notification's title in the confirmation reply.
    messagePort.postMessage('The notification has been clicked on.');
});
