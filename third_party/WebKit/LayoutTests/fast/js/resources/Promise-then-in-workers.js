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
  shouldBeFalse('thisInFulfillCallback === firstPromise');
  shouldBeFalse('thisInFulfillCallback === secondPromise');
  shouldBeTrue('thisInFulfillCallback === global');
  global.result = result;
  shouldBeEqualToString('result', 'hello');
  finishJSTest();
});

shouldBeFalse('thisInInit === firstPromise');
shouldBeTrue('thisInInit === global');
shouldBeTrue('firstPromise instanceof Promise');
shouldBeTrue('secondPromise instanceof Promise');

shouldThrow('firstPromise.then(null)', '"TypeError: onFulfilled must be a function or undefined"');
shouldThrow('firstPromise.then(undefined, null)', '"TypeError: onRejected must be a function or undefined"');
shouldThrow('firstPromise.then(37)', '"TypeError: onFulfilled must be a function or undefined"');

resolve('hello');


