if (self.importScripts) {
  importScripts('../resources/fetch-test-helpers.js');
  importScripts('/serviceworker/resources/fetch-access-control-util.js');
}

var referer;

if (self.importScripts) {
  // fetch/workers or fetch/serviceworker
  referer = 'http://127.0.0.1:8000/fetch/script-tests/fetch-access-control.js';
} else if(location.pathname ==
    '/fetch/serviceworker-proxied/fetch-access-control.html') {
  // fetch/serviceworker-proxied
  referer = WORKER_URL;
} else {
  // fetch/window
  referer = 'http://127.0.0.1:8000/fetch/window/fetch-access-control.html';
}

var TEST_TARGETS = [
  [BASE_URL + 'method=GET',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, authCheck1]],
  [BASE_URL + 'method=GET&headers={}',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET]],
  [BASE_URL + 'method=GET&headers=CUSTOM',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, noCustomHeader]],
  [BASE_URL + 'method=POST&headers=CUSTOM',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsPOST, noCustomHeader]],
  [BASE_URL + 'method=PUT',
   [fetchError]],
  [BASE_URL + 'method=XXX',
   [fetchError]],

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

  [BASE_URL + 'mode=no-cors&method=GET',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, authCheck1]],
  [BASE_URL + 'mode=no-cors&method=GET&headers={}',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET]],
  [BASE_URL + 'mode=no-cors&method=GET&headers=CUSTOM',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, noCustomHeader]],
  [BASE_URL + 'mode=no-cors&method=POST&headers=CUSTOM',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsPOST, noCustomHeader]],
  [BASE_URL + 'mode=no-cors&method=PUT',
   [fetchError]],
  [BASE_URL + 'mode=no-cors&method=XXX',
   [fetchError]],

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
  [BASE_URL,
   [fetchResolved],
   [checkJsonpHeader.bind(this, 'Referer', referer)]],

  // Auth check
  [BASE_URL + 'Auth',
   [fetchResolved, hasBody], [authCheck1]],
  [BASE_URL + 'Auth&credentials=omit',
   [fetchResolved, hasBody], [checkJsonpError]],
  [BASE_URL + 'Auth&credentials=include',
   [fetchResolved, hasBody], [authCheck1]],
  [BASE_URL + 'Auth&credentials=same-origin',
   [fetchResolved, hasBody], [authCheck1]],

  [BASE_URL + 'Auth&mode=no-cors&credentials=omit',
   [fetchResolved, hasBody], [checkJsonpError]],
  [BASE_URL + 'Auth&mode=no-cors&credentials=include',
   [fetchResolved, hasBody], [authCheck1]],
  [BASE_URL + 'Auth&mode=no-cors&credentials=same-origin',
   [fetchResolved, hasBody], [authCheck1]],

  [BASE_URL + 'Auth&mode=same-origin&credentials=omit',
   [fetchResolved, hasBody], [checkJsonpError]],
  [BASE_URL + 'Auth&mode=same-origin&credentials=include',
   [fetchResolved, hasBody], [authCheck1]],
  [BASE_URL + 'Auth&mode=same-origin&credentials=same-origin',
   [fetchResolved, hasBody], [authCheck1]],

  [BASE_URL + 'Auth&mode=cors&credentials=omit',
   [fetchResolved, hasBody], [checkJsonpError]],
  [BASE_URL + 'Auth&mode=cors&credentials=include',
   [fetchResolved, hasBody], [authCheck1]],
  [BASE_URL + 'Auth&mode=cors&credentials=same-origin',
   [fetchResolved, hasBody], [authCheck1]],

  [OTHER_BASE_URL + 'Auth',
   [fetchResolved, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([authCheck2])],
  [OTHER_BASE_URL + 'Auth&credentials=omit',
   [fetchResolved, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([checkJsonpError])],
  [OTHER_BASE_URL + 'Auth&credentials=include',
   [fetchResolved, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([authCheck2])],
  [OTHER_BASE_URL + 'Auth&credentials=same-origin',
   [fetchResolved, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([authCheck2])],

  [OTHER_BASE_URL + 'Auth&mode=no-cors&credentials=omit',
   [fetchResolved, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([checkJsonpError])],
  [OTHER_BASE_URL + 'Auth&mode=no-cors&credentials=include',
   [fetchResolved, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([authCheck2])],
  [OTHER_BASE_URL + 'Auth&mode=no-cors&credentials=same-origin',
   [fetchResolved, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([authCheck2])],

  [OTHER_BASE_URL + 'Auth&mode=same-origin&credentials=omit',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=same-origin&credentials=include',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=same-origin&credentials=same-origin',
   [fetchRejected]],

  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=omit',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=same-origin',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include&ACAOrigin=*',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include&ACAOrigin=http://127.0.0.1:8000',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include&ACAOrigin=*&ACACredentials=true',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include&ACAOrigin=http://127.0.0.1:8000&ACACredentials=true',
   [fetchResolved, hasBody], [authCheck2]]
];

if (self.importScripts) {
  executeTests(TEST_TARGETS);
  done();
}
