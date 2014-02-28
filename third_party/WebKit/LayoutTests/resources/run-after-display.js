function runAfterDisplay(callback) {
    window.requestAnimationFrame(function() {
        // At this point, only the animate has happened, but no compositing
        // or layout.  Use a timeout for the callback so that notifyDone
        // can be called inside of it.
        window.setTimeout(callback, 0);
    });
}
