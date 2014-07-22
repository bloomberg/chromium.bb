function handleReferrer(event) {
    event.respondWith(new Response(new Blob(["Referrer: " + event.request.referrer])));
}

function handleNullBody(event) {
    event.respondWith(new Response(null));
}

function handleReject(event) {
    event.respondWith(new Promise(function(resolve, reject) {
        reject('rejected!');
    }));
}

function handleUnresolved(event) {
    event.respondWith(new Promise(function(resolve, reject) { }));
}

function handleFetch(event) {
    event.respondWith(fetch('other.html'));
}

self.addEventListener('fetch', function(event) {
    var url = event.request.url;
    var handlers = [
        { pattern: 'referrer', fn: handleReferrer },
        { pattern: '?ignore', fn: function() {} },
        { pattern: '?null', fn: handleNullBody },
        { pattern: '?reject', fn: handleReject },
        { pattern: '?unresolved', fn: handleUnresolved },
        { pattern: '?fetch', fn: handleFetch }
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
