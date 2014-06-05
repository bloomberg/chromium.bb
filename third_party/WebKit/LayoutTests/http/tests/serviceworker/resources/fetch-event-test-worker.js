function handleHelloWorld(event) {
    event.respondWith(new Response(new Blob(["hello, world"])));
}

function handleNullBody(event) {
    event.respondWith(new Response(null));
}

function handleReject(event) {
    event.respondWith(new Promise(function(resolve, reject) {
        reject('rejected!');
    }));
}

self.addEventListener('fetch', function(event) {
    var url = event.request.url;
    var handlers = [
        { pattern: 'helloworld', fn: handleHelloWorld },
        { pattern: '?ignore', fn: function() {} },
        { pattern: '?null', fn: handleNullBody },
        { pattern: '?reject', fn: handleReject }
    ];

    var handler = null;
    for (var i = 0; i < handlers.length; ++i) {
       if (url.indexOf(handlers[i].pattern) != -1) {
           handler = handlers[i];
           break;
       }
    }

    if (handler)
        handler.fn(event);
    else
        event.respondWith(new Response(new Blob(['Service Worker got an unexpected request: ' + url])));
});
