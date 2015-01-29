if (self.importScripts) {
  importScripts('../resources/fetch-test-helpers.js');
}

var BASE_URL =
  'http://127.0.0.1:8000/serviceworker/resources/fetch-access-control.php?';

async_test(function(t) {
    fetch(BASE_URL + 'ACAOrigin=*&label=same-origin', {mode: 'same-origin'})
      .then(
        t.unreached_func('should be blocked as mixed content (same-origin)'),
        function() { t.done(); })
      .catch(unreached_rejection(t));
  }, 'Block fetch() as mixed content (same-origin)');

async_test(function(t) {
    fetch(BASE_URL + 'ACAOrigin=*&label=cors', {mode: 'cors'})
      .then(t.unreached_func(
        'should be blocked as mixed content (cors)'),
        function() { t.done(); })
      .catch(unreached_rejection(t));
  }, 'Block fetch() as mixed content (cors)');

async_test(function(t) {
    fetch(BASE_URL + 'ACAOrigin=*&label=no-cors', {mode: 'no-cors'})
      .then(t.unreached_func(
        'should be blocked as mixed content (no-cors)'),
        function() { t.done(); })
      .catch(unreached_rejection(t));
  }, 'Block fetch() as mixed content (no-cors)');

done();
