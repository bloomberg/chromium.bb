importScripts('./js-test-pre.js');

description('Test Promise.');

var global = this;

global.jsTestIsAsync = true;
var reject;

var firstPromise = new Promise(function(newResolve, newReject) {
  global.thisInInit = this;
  reject = newReject;
});

var secondPromise = firstPromise.catch(function(result) {
  global.thisInFulfillCallback = this;
  shouldBeFalse('thisInFulfillCallback === firstPromise');
  shouldBeTrue('thisInFulfillCallback === secondPromise');
  global.result = result;
  shouldBeEqualToString('result', 'hello');
  return 'bye';
});

secondPromise.then(function(result) {
  global.result = result;
  shouldBeEqualToString('result', 'bye');
  testPassed('fulfilled');
  finishJSTest();
}, function() {
  testFailed('rejected');
  finishJSTest();
}, function() {
});

shouldBeTrue('thisInInit === firstPromise');
shouldBeTrue('firstPromise instanceof Promise');
shouldBeTrue('secondPromise instanceof Promise');

shouldThrow('firstPromise.catch(null)', '"TypeError: rejectCallback must be a function or undefined"');
shouldThrow('firstPromise.catch(37)', '"TypeError: rejectCallback must be a function or undefined"');

reject('hello');

