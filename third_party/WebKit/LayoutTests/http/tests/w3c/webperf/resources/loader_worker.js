var numRequests = 0;
var numComplete = 0;

function checkDone(callback) {
  ++numComplete;
  if (numComplete == numRequests) {
    callback();
  }
}

function makeRequest(request, callback) {
  var xhr = new XMLHttpRequest;
  xhr.open('get', request, true);
  xhr.onreadystatechange = function() {
    if (xhr.readyState == 4) {
      checkDone(callback);
    }
  }
  xhr.send();
}

function loadRequests(event, callback) {
  var requests = event.data;
  numComplete = 0;
  numRequests = requests.length;
  for (var i in requests) {
    var request = requests[i];
    makeRequest(request, callback);
  }
}

self.onmessage = function(event) {
  loadRequests(event,
               function(results) {
                 postMessage(results);
                 self.close();
               });
};

self.addEventListener("connect", function(event) {
  var port = event.ports[0];
  port.onmessage = function(event) {
    loadRequests(event,
                 function(results) {
                   port.postMessage(results);
                 });
  };
});
