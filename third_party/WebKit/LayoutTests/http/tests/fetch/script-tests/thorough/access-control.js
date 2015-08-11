// OPTIONS: ,-base-https-other-https
if (self.importScripts) {
  importScripts('/fetch/resources/fetch-test-helpers.js');
  importScripts('/fetch/resources/thorough-util.js');
}

var referer;

if (self.importScripts) {
  // fetch/workers or fetch/serviceworker
  referer = BASE_ORIGIN +
            '/fetch/script-tests/thorough/access-control.js?' + TEST_OPTIONS;
} else if(location.pathname.startsWith('/fetch/serviceworker-proxied/')) {
  // fetch/serviceworker-proxied
  referer = WORKER_URL;
} else {
  // fetch/window
  referer = BASE_ORIGIN +
            '/fetch/window/thorough/access-control' + TEST_OPTIONS + '.html';
}

var TEST_TARGETS = [
  // Test mode=same-origin.
  [BASE_URL + 'mode=same-origin&method=GET',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, authCheck1]],
  [BASE_URL + 'mode=same-origin&method=GET&headers={}',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET]],
  [BASE_URL + 'mode=same-origin&method=GET&headers=CUSTOM',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, hasCustomHeader]],
  [BASE_URL + 'mode=same-origin&method=POST&headers=CUSTOM',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsPOST, hasCustomHeader]],
  [BASE_URL + 'mode=same-origin&method=PUT&headers=CUSTOM',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsPUT, hasCustomHeader]],
  [BASE_URL + 'mode=same-origin&method=XXX&headers=CUSTOM',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsXXX, hasCustomHeader]],

  // Test mode=cors.
  [BASE_URL + 'mode=cors&method=GET',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, authCheck1]],
  [BASE_URL + 'mode=cors&method=GET&headers={}',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET]],
  [BASE_URL + 'mode=cors&method=GET&headers=CUSTOM',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, hasCustomHeader]],
  [BASE_URL + 'mode=cors&method=POST&headers=CUSTOM',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsPOST, hasCustomHeader]],
  [BASE_URL + 'mode=cors&method=PUT&headers=CUSTOM',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsPUT, hasCustomHeader]],
  [BASE_URL + 'mode=cors&method=XXX&headers=CUSTOM',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsXXX, hasCustomHeader]],

  // Referer check
  [BASE_URL + 'mode=same-origin',
   [fetchResolved],
   [checkJsonpHeader.bind(this, 'Referer', referer)]],
];

if (self.importScripts) {
  executeTests(TEST_TARGETS);
  done();
}
