var message_id = 1;
onmessage = function(event) {
  doWork();
};

function doWork() {
  postMessage("Message #" + message_id++);

  var ts = Date.now();
  while (true) {
    if (Date.now() - ts > 1000) {
        ts = Date.now();
        postMessage("Message #" + message_id++);
    }
  }
}

