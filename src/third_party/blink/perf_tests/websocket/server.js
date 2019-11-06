const ws = require('ws')
const ws_server = new ws.Server({ host: '0.0.0.0', port: 8001 });
const getNow = () => {
  const date = new Date();
  return date.toLocaleTimeString() + "." + date.getMilliseconds();
};
console.log(getNow() + " WebSocket server started.");
const arrayBuf = 1000*1000; // 1MB
const totalIter = 100;
ws_server.on('connection', function(ws_socket) {
  console.log(getNow() + " Connection established.");
  const data = new ArrayBuffer(arrayBuf);
  for (let i = 0; i < totalIter; i++) {
    ws_socket.send(data, {binary: true});
  }
  ws_socket.close();
  console.log(getNow() + " Connection closed.");
});