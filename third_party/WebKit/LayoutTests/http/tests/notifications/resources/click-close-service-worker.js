// Sends a message when the notificationclick event gets received, and closes the
// notification which was shown from that event.
var messagePort = null;

addEventListener('message', function(event) {
    messagePort = event.data;
    messagePort.postMessage('ready');
});

addEventListener('notificationclick', function(event) {
    event.notification.close();
    messagePort.postMessage('Clicked on Notification: ' + event.notification.title);
});
