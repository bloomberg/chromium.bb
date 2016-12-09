let port;
let received = false;

function reportFailure(details) {
  port.postMessage('FAIL: ' + details);
}

onmessage = event => {
  port = event.source;

  const ws = new WebSocket('ws://localhost:8880/echo');
  ws.onopen = () => {
    ws.send('Hello');
  };
  ws.onmessage = msg => {
    if (msg.data !== 'Hello') {
      reportFailure('Unexpected reply: ' + msg.data);
      return;
    }

    received = true;
    ws.close();
  };
  ws.onclose = () => {
    if (!received) {
      reportFailure('Closed before receiving reply');
      return;
    }

    port.postMessage('PASS');
  };
  ws.onerror = () => {
    reportFailure('Got an error event');
  };
};
