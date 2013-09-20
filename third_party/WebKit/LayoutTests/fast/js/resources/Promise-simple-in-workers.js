importScripts('./js-test-pre.js');

description('Test Promise.');

var global = this;

global.jsTestIsAsync = true;

var resolve;

var firstPromise = new Promise(function(newResolve) {
  global.thisInInit = this;
  resolve = newResolve;
});

var secondPromise = firstPromise.then(function(result) {
  global.thisInFulfillCallback = this;
  shouldBeTrue('thisInFulfillCallback === secondPromise');
  global.result = result;
  shouldBeEqualToString('result', 'hello');
  finishJSTest();
});

shouldBeTrue('thisInInit === firstPromise');

resolve('hello');
