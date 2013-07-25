self.onerror = function (message, filename, lineno, column) {
    postMessage({ 'message': message, 'filename': filename, 'lineno': lineno, 'column': column });
};

importScripts('http://localhost:8000/workers/resources/worker-importScripts-throw.js');
