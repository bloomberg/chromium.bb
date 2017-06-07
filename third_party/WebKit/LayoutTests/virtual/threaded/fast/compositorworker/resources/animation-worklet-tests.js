function runInAnimationWorklet(code) {
  return window.animationWorklet.addModule(
    URL.createObjectURL(new Blob([code], {type: 'text/javascript'}))
  );
}

// Wait for two main thread frames to guarantee that compositor has produced
// at least one frame.
function waitTwoAnimationFrames(callback){
  window.requestAnimationFrame( _=> {
    window.requestAnimationFrame( _ => {
      callback();
    })
  });
};

// Load test cases in worklet context in sequence and wait until they are resolved.
function runTests(testcases) {
  if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
  }

  const runInSequence = testcases.reduce((chain, testcase) => {
    return chain.then( _ => {
        return runInAnimationWorklet(testcase);
    });
  }, Promise.resolve());

  runInSequence.then(_ => {
    // Wait to guarantee that compositor frame is produced before finishing.
    // This ensure we see at least one |animate| call in the animation worklet.
    waitTwoAnimationFrames(_ => {
      if (window.testRunner)
        testRunner.notifyDone();
     });
  });
}

const testcases = Array.prototype.map.call(document.querySelectorAll('script[type$=worklet]'), ($el) => {
  return $el.textContent;
});

runTests(testcases);
