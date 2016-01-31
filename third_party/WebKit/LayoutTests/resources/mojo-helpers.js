/*
 * mojo-helpers contains extensions to testharness.js useful for consuming
 * and mocking Mojo services directly within test code.
 */
'use strict';

// Runs a promise_test which depends on the Mojo system API modules available to
// all layout tests. The test implementation function is called with an Object
// that exposes common Mojo module interfaces.
function mojo_test(func, name, properties) {
  // Fix up the global window.define, since all baked-in Mojo modules expect to
  // find it there.
  window.define = mojo.define;

  promise_test(() => {
    return new Promise((resolve, reject) => {
      define('Mojo layout test module: ' + name, [
        'mojo/public/js/core',
        'mojo/public/js/router',
        'content/public/renderer/service_provider',
      ], (core, router, serviceProvider) => {
        try {
          resolve(func({
            core: core,
            router: router,

            // |serviceProvider| is a bit of a misnomer. It should probably be
            // called |serviceRegistry|, so let's call it that here.
            serviceRegistry: serviceProvider,
          }));
        } catch (e) {
          reject(e);
        }
      });
    });
  }, name, properties);
}

// Polls aggressively for a message to become available on a pipe.
function mojo_wait_for_incoming_message(mojo, pipe) {
  return new Promise((resolve, reject) => {
    let wait = () => {
      let result = mojo.core.readMessage(pipe, 0);
      if (result.result === mojo.core.RESULT_SHOULD_WAIT) {
        setTimeout(wait);
        return;
      }

      if (result.result !== mojo.core.RESULT_OK) {
        reject(result.result);
        return;
      }

      resolve({ buffer: result.buffer, handles: result.handles });
    };

    wait();
  });
};
