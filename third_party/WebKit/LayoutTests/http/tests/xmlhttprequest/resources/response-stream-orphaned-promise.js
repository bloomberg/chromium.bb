var global = this;
if (global.importScripts) {
    // Worker case
    importScripts('/resources/testharness.js');
}

function fetchBody(url) {
    return new Promise(function(resolve, reject) {
        var xhr = new XMLHttpRequest;
        xhr.responseType = 'stream';
        var visited = false;
        xhr.onreadystatechange = function() {
            if (xhr.readyState !== xhr.LOADING || visited)
                return;
            visited = true;
            // Note that all provided urls have empty bodies, so
            // we don't have to read the data.
            xhr.response.getReader().closed.then(resolve, reject);
        };
        xhr.open('GET', url);
        xhr.send();
    });
}

promise_test(function() {
    var url = '/xmlhttprequest/resources/slow-empty-response.cgi';
    return fetchBody(url);
}, 'check if |closed| gets resolved without stream reference');

promise_test(function() {
    var url = '/xmlhttprequest/resources/slow-failure.cgi';
    return fetchBody(url).then(function() {
        assert_unreached('resolved unexpectedly');
    }, function() {
        // rejected as expected
    });
}, 'check if |closed| gets rejected without explicit stream reference');

if (global.importScripts) {
    // Worker case
    done();
}
