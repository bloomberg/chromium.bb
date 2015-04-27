

Polymer.Async = (function() {
  
  var currVal = 0;
  var lastVal = 0;
  var callbacks = [];
  var twiddle = document.createTextNode('');

  function runAsync(callback, waitTime) {
    if (waitTime > 0) {
      return ~setTimeout(callback, waitTime);
    } else {
      twiddle.textContent = currVal++;
      callbacks.push(callback);
      return currVal - 1;
    }
  }

  function cancelAsync(handle) {
    if (handle < 0) {
      clearTimeout(~handle);
    } else {
      var idx = handle - lastVal;
      if (idx >= 0) {
        if (!callbacks[idx]) {
          throw 'invalid async handle: ' + handle;
        }
        callbacks[idx] = null;
      }
    }
  }

  function atEndOfMicrotask() {
    var len = callbacks.length;
    for (var i=0; i<len; i++) {
      var cb = callbacks[i];
      if (cb) {
        cb();
      }
    }
    callbacks.splice(0, len);
    lastVal += len;
  }

  new (window.MutationObserver || JsMutationObserver)(atEndOfMicrotask)
    .observe(twiddle, {characterData: true})
    ;
  
  // exports 

  return {
    run: runAsync,
    cancel: cancelAsync
  };
  
})();

