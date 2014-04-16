var savedScope = self.scope;

self.addEventListener('message', function(e) {
    e.ports[0].postMessage({
        initialScope: savedScope,
        currentScope: self.scope
    });
});
