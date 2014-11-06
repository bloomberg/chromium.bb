var SCOPE = 'resources/fetch-access-control-iframe.html';
var BASE_URL = 'http://127.0.0.1:8000/serviceworker/resources/fetch-access-control.php?';
var OTHER_BASE_URL = 'http://localhost:8000/serviceworker/resources/fetch-access-control.php?';
var REDIRECT_URL = 'http://127.0.0.1:8000/serviceworker/resources/redirect.php?Redirect=';
var IFRAME_URL = 'http://127.0.0.1:8000/serviceworker/resources/fetch-access-control-iframe.html';
var WORKER_URL = 'http://127.0.0.1:8000/serviceworker/resources/fetch-access-control-worker.js';
var IFRAME_ORIGIN = 'http://127.0.0.1:8000';

// Functions to check the result from the ServiceWorker.
var checkFetchResult = function(expected, url, data) {
  assert_equals(data.fetchResult, expected, url + ' should be ' + expected);
};
var checkFetchResponseBody = function(hasBody, url, data) {
  assert_equals(data.fetchResult,
                'resolved',
                'fetchResult must be resolved. url: ' + url);
  if (hasBody) {
    assert_not_equals(data.body, '',
                      'response must have body. url: ' + url);
  } else {
    assert_equals(data.body, '',
                  'response must not have body. url: ' + url);
  }
};
var checkFetchResponseHeader = function(name, expected, url, data) {
  assert_equals(data.fetchResult,
                'resolved',
                'fetchResult must be resolved. url: ' + url);
  var exist = false;
  for (var i = 0; i < data.headers.length; ++i) {
    if (data.headers[i][0] === name) {
      exist = true;
    }
  }
  assert_equals(exist,
                expected,
                'header check failed url: ' + url + ' name: ' + name);
};
var checkFetchResponseType = function(type, url, data) {
  assert_equals(data.fetchResult,
                'resolved',
                'fetchResult must be resolved. url = ' + url);
  assert_equals(data.type,
                type,
                'type must match. url: ' + url);
};
var fetchIgnored = checkFetchResult.bind(this, 'ignored');
var fetchResolved = checkFetchResult.bind(this, 'resolved');
var fetchRejected = checkFetchResult.bind(this, 'rejected');
var fetchError = checkFetchResult.bind(this, 'error');
var hasBody = checkFetchResponseBody.bind(this, true);
var noBody = checkFetchResponseBody.bind(this, false);
var hasContentLength =
    checkFetchResponseHeader.bind(this, 'content-length', true);
var noContentLength =
    checkFetchResponseHeader.bind(this, 'content-length', false);
var hasServerHeader =
    checkFetchResponseHeader.bind(this, 'x-serviceworker-serverheader', true);
var noServerHeader =
    checkFetchResponseHeader.bind(this, 'x-serviceworker-serverheader', false);
var typeBasic = checkFetchResponseType.bind(this, 'basic');
var typeCors = checkFetchResponseType.bind(this, 'cors');
var typeOpaque = checkFetchResponseType.bind(this, 'opaque');

// Functions to check the result of JSONP which is evaluated in
// fetch-access-control-iframe.html by appending <script> element.
var checkJsonpResult = function(expected, url, data) {
  assert_equals(data.jsonpResult,
                expected,
                url + ' jsonpResult should match');
};
var checkJsonpHeader = function(name, value, url, data) {
  assert_equals(data.jsonpResult,
                'success',
                url + ' jsonpResult must be success');
  assert_equals(data.headers[name],
                value,
                'Request header check failed url:' + url + ' name:' + name);
};
var checkJsonpMethod = function(method, url, data) {
  assert_equals(data.jsonpResult,
                'success',
                url + ' jsonpResult must be success');
  assert_equals(data.method,
                method,
                'Method must match url:' + url);
};
var checkJsonpAuth = function(username, password, url, data) {
  assert_equals(data.jsonpResult,
                'success',
                url + ' jsonpResult must be success');
  assert_equals(data.username,
                username,
                'Username must match. url: ' + url);
  assert_equals(data.password,
                password,
                'Password must match. url: ' + url);
  assert_equals(data.cookie,
                username,
                'Cookie must match. url: ' + url);
};
var checkJsonpError = checkJsonpResult.bind(this, 'error');
var checkJsonpSuccess = checkJsonpResult.bind(this, 'success');
var hasCustomHeader =
    checkJsonpHeader.bind(this, 'x-serviceworker-test', 'test');
var noCustomHeader =
    checkJsonpHeader.bind(this, 'x-serviceworker-test', undefined);
var methodIsGET = checkJsonpMethod.bind(this, 'GET');
var methodIsPOST = checkJsonpMethod.bind(this, 'POST');
var methodIsPUT = checkJsonpMethod.bind(this, 'PUT');
var methodIsXXX = checkJsonpMethod.bind(this, 'XXX');
var authCheckNone = checkJsonpAuth.bind(this, 'undefined', 'undefined');
var authCheck1 = checkJsonpAuth.bind(this, 'username1', 'password1');
var authCheck2 = checkJsonpAuth.bind(this, 'username2', 'password2');

function executeTests(test, test_targets) {
  test.step(function() {
    var login1 =
        test_login(test, 'http://127.0.0.1:8000', 'username1', 'password1');
    var login2 =
        test_login(test, 'http://localhost:8000', 'username2', 'password2');
    var workerScript = 'resources/fetch-access-control-worker.js';
    var worker = undefined;
    var frameWindow = {};
    var counter = 0;
    window.addEventListener('message', test.step_func(onMessage), false);

    Promise.all([login1, login2])
      .then(function() {
          return service_worker_unregister_and_register(test,
                                                        workerScript,
                                                        SCOPE);
        })
      .then(function(registration) {
          return wait_for_update(test, registration);
        })
      .then(function(sw) {
          worker = sw;
          var messageChannel = new MessageChannel();
          messageChannel.port1.onmessage = test.step_func(onWorkerMessage);
          sw.postMessage(
              {port: messageChannel.port2}, [messageChannel.port2]);
          return wait_for_state(test, sw, 'activated');
        })
      .then(function() {
          return with_iframe(SCOPE);
        })
      .then(function(frame) {
          frameWindow = frame.contentWindow;
          // Start tests.
          loadNext();
        })
      .catch(unreached_rejection(test));

    var readyFromWorkerReceived = undefined;
    var resultFromWorkerReceived = undefined;
    var resultFromIframeReceived = undefined;

    function onMessage(e) {
      // The message is sent from fetch-access-control-iframe.html in report()
      // which is called by appending <script> element which source code is
      // generated by fetch-access-control.php.
      if (TEST_TARGETS[counter][2]) {
        TEST_TARGETS[counter][2].forEach(function(checkFunc) {
          checkFunc.call(this,
                         TEST_TARGETS[counter][0],
                         e.data);
        });
      }
      resultFromIframeReceived();
    }

    function onWorkerMessage(e) {
      // The message is sent from the ServiceWorker.
      var message = e.data;
      if (message.msg === 'READY') {
        readyFromWorkerReceived();
        return;
      }
      TEST_TARGETS[counter][1].forEach(function(checkFunc) {
        checkFunc.call(this,
                       TEST_TARGETS[counter][0],
                       message);
      });
      resultFromWorkerReceived();
    }

    function loadNext() {
      var workerPromise = new Promise(function(resolve, reject) {
        resultFromWorkerReceived = resolve;
      });
      var iframePromise = new Promise(function(resolve, reject) {
        resultFromIframeReceived = resolve;
      });
      Promise.all([workerPromise, iframePromise])
        .then(test.step_func(function() {
            ++counter;
            if (counter === TEST_TARGETS.length) {
              service_worker_unregister_and_done(test, SCOPE);
            } else {
              loadNext();
            }
          }));
      (new Promise(function(resolve, reject) {
        readyFromWorkerReceived = resolve;
        worker.postMessage({msg: 'START TEST CASE'});
      }))
        .then(test.step_func(function() {
            frameWindow.postMessage(
                {url: TEST_TARGETS[counter][0]},
                IFRAME_ORIGIN);
          }));
    }
  });
}
