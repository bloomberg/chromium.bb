self.onerror = function (message, filename, lineno, column) {
    postMessage({ 'message': message, 'filename': filename, 'lineno': lineno, 'column': column });
};

importScripts('/workers/resources/worker-importScripts-throw.js');
