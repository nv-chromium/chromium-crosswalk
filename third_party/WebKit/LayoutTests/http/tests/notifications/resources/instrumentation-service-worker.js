importScripts('/resources/testharness-helpers.js');

// Copies the serializable attributes of |notification|.
function cloneNotification(notification) {
    return JSON.parse(stringifyDOMObject(notification));
}

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

            case 'get':
                var filter = {};
                if (typeof (event.data.filter) !== 'undefined')
                    filter = event.data.filter;

                registration.getNotifications(filter).then(function(notifications) {
                    var clonedNotifications = [];
                    for (var notification of notifications)
                        clonedNotifications.push(cloneNotification(notification));

                    messagePort.postMessage({ command: event.data.command,
                                              success: true,
                                              notifications: clonedNotifications });
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
    var notificationCopy = cloneNotification(event.notification);

    // Notifications containing "ACTION:CLOSE" in their message will be closed
    // immediately by the Service Worker.
    if (event.notification.body.indexOf('ACTION:CLOSE') != -1)
        event.notification.close();

    // Notifications containing "ACTION:OPENWINDOW" in their message will attempt
    // to open a new window for an example URL.
    if (event.notification.body.indexOf('ACTION:OPENWINDOW') != -1)
        event.waitUntil(clients.openWindow('https://example.com/'));

    messagePort.postMessage({ command: 'click',
                              notification: notificationCopy,
                              action: event.action });
});
