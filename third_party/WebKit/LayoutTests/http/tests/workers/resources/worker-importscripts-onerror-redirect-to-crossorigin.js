self.onerror = function (message, filename, lineno, column) {
    postMessage({ 'message': message, 'filename': filename, 'lineno': lineno, 'column': column });
};

var differentRedirectOrigin = "/resources/redirect.php?url=http://localhost:8000/workers/resources/worker-importScripts-throw.js";
importScripts(differentRedirectOrigin)
