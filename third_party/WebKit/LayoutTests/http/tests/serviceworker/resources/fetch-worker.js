self.onmessage = function(e) {
    var message = e.data;
    if ('port' in message) {
        port = message.port;
        doNextFetchTest(port);
    }
};

var testTargets = [
    'other.html',
    'http://',
    'http://www.example.com/foo',
    'fetch-status.php?status=200',
    'fetch-status.php?status=404'
];

function doNextFetchTest(port) {

    function runInfiniteFetchLoop() {
      fetch('dummy.html')
        .then(function() { runInfiniteFetchLoop(); });
    }

    if (testTargets.length == 0) {
      // Destroying the execution context while fetch is happening
      // should not cause a crash.
      runInfiniteFetchLoop();

      port.postMessage('quit');
      return;
    }
    var target = testTargets.shift();
    fetch(target)
    .then(function(response) {
        port.postMessage('Resolved: ' + target + ' [' + response.status + ']' + response.statusText);
        doNextFetchTest(port);
    }).catch(function(e) {
        port.postMessage('Rejected: ' + target + ' : '+ e.message);
        doNextFetchTest(port);
    });
};
