if (self.importScripts) {
  importScripts('../resources/fetch-test-helpers.js');
  importScripts('/serviceworker/resources/fetch-access-control-util.js');
}

var TEST_TARGETS = [
  // CORS test
  [OTHER_BASE_URL + 'method=GET&headers=CUSTOM',
   [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([methodIsGET, noCustomHeader, authCheck2])],
  [OTHER_BASE_URL + 'method=POST&headers=CUSTOM',
   [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([methodIsPOST, noCustomHeader])],
  [OTHER_BASE_URL + 'method=PUT&headers=CUSTOM',
   [fetchError]],
  [OTHER_BASE_URL + 'method=XXX&headers=CUSTOM',
   [fetchError]],

  [OTHER_BASE_URL + 'mode=same-origin&method=GET', [fetchRejected]],
  [OTHER_BASE_URL + 'mode=same-origin&method=POST', [fetchRejected]],
  [OTHER_BASE_URL + 'mode=same-origin&method=PUT', [fetchRejected]],
  [OTHER_BASE_URL + 'mode=same-origin&method=XXX', [fetchRejected]],

  [OTHER_BASE_URL + 'mode=no-cors&method=GET&headers=CUSTOM',
   [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([methodIsGET, noCustomHeader, authCheck2])],
  [OTHER_BASE_URL + 'mode=no-cors&method=POST&headers=CUSTOM',
   [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([methodIsPOST, noCustomHeader])],
  [OTHER_BASE_URL + 'mode=no-cors&method=PUT&headers=CUSTOM',
   [fetchError]],
  [OTHER_BASE_URL + 'mode=no-cors&method=XXX&headers=CUSTOM',
   [fetchError]],

  // method=GET

  // CORS check
  // https://fetch.spec.whatwg.org/#concept-cors-check
  // Tests for Access-Control-Allow-Origin header.
  [OTHER_BASE_URL + 'mode=cors&method=GET',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=GET&ACAOrigin=*',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  [OTHER_BASE_URL + 'mode=cors&method=GET&ACAOrigin=' + BASE_ORIGIN,
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET]],
  [OTHER_BASE_URL + 'mode=cors&method=GET&ACAOrigin=' + BASE_ORIGIN +
   ',http://www.example.com',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=GET&ACAOrigin=http://www.example.com',
   [fetchRejected]],

  // CORS filtered response
  // https://fetch.spec.whatwg.org/#concept-filtered-response-cors
  // Tests for Access-Control-Expose-Headers header.
  [OTHER_BASE_URL + 'mode=cors&method=GET&ACAOrigin=*&ACEHeaders=X-ServiceWorker-ServerHeader',
   [fetchResolved, noContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsGET]],
  [OTHER_BASE_URL + 'mode=cors&method=GET&ACAOrigin=' + BASE_ORIGIN +
   '&ACEHeaders=X-ServiceWorker-ServerHeader',
   [fetchResolved, noContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsGET]],
  [OTHER_BASE_URL + 'mode=cors&method=GET&ACAOrigin=*&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsGET]],
  [OTHER_BASE_URL + 'mode=cors&method=GET&ACAOrigin=' + BASE_ORIGIN +
   '&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsGET]],

  // CORS preflight fetch
  // https://fetch.spec.whatwg.org/#cors-preflight-fetch
  // Tests for Access-Control-Allow-Headers header.
  [OTHER_BASE_URL + 'mode=cors&method=GET&headers=CUSTOM',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=GET&headers=CUSTOM&ACAOrigin=*',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=GET&headers=CUSTOM&ACAOrigin=' +
   BASE_ORIGIN,
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=GET&headers=CUSTOM&ACAOrigin=*&ACAHeaders=x-serviceworker-test',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=GET&headers=CUSTOM&ACAOrigin=' +
   BASE_ORIGIN + '&ACAHeaders=x-serviceworker-test',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=GET&headers=CUSTOM&ACAOrigin=*&ACAHeaders=x-serviceworker-test&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsGET, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=GET&headers=CUSTOM&ACAOrigin=' +
   BASE_ORIGIN +
   '&ACAHeaders=x-serviceworker-test&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsGET, hasCustomHeader]],

  // Test that Access-Control-Allow-Headers is checked in CORS preflight fetch.
  [OTHER_BASE_URL + 'mode=cors&method=GET&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAHeaders=x-serviceworker-test&PreflightTest=200',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=GET&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&ACAHeaders=x-serviceworker-test&PreflightTest=200',
   [fetchRejected]],

  // Test that CORS check is done in both preflight and main fetch.
  [OTHER_BASE_URL + 'mode=cors&method=GET&headers=CUSTOM&ACAOrigin=*&PACAHeaders=x-serviceworker-test&PreflightTest=200',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=GET&headers=CUSTOM&PACAOrigin=*&PACAHeaders=x-serviceworker-test&PreflightTest=200',
   [fetchRejected]],

  // Test that Access-Control-Expose-Headers of CORS preflight is ignored.
  [OTHER_BASE_URL + 'mode=cors&method=GET&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAHeaders=x-serviceworker-test&PACEHeaders=Content-Length, X-ServiceWorker-ServerHeader&PreflightTest=200',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, hasCustomHeader]],

  // Test that CORS preflight with Status 2XX succeeds.
  [OTHER_BASE_URL + 'mode=cors&method=GET&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAHeaders=x-serviceworker-test&PreflightTest=201',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, hasCustomHeader]],

  // Test that CORS preflight with Status other than 2XX fails.
  // https://crbug.com/452394
  [OTHER_BASE_URL + 'mode=cors&method=GET&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAHeaders=x-serviceworker-test&PreflightTest=301',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=GET&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAHeaders=x-serviceworker-test&PreflightTest=401',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=GET&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAHeaders=x-serviceworker-test&PreflightTest=500',
   [fetchRejected]],

  // Test CORS preflight with multiple request headers.
  [OTHER_BASE_URL + 'mode=cors&method=GET&headers=CUSTOM2&ACAOrigin=*&PACAOrigin=*&PACAHeaders=x-servicEworker-u, x-servicEworker-ua, x-servicewOrker-test, x-sErviceworker-s, x-sErviceworker-v&PreflightTest=200',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, hasCustomHeader2]],
  [OTHER_BASE_URL + 'mode=cors&method=GET&headers=CUSTOM2&ACAOrigin=*&PACAOrigin=*&PACAHeaders=x-servicewOrker-test&PreflightTest=200',
   [fetchRejected]],

  // Test request headers sent in CORS preflight requests.
  [OTHER_BASE_URL + 'mode=cors&method=GET&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAHeaders=x-serviceworker-test&PreflightTest=200&PACRMethod=GET&PACRHeaders=x-serviceworker-test',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, hasCustomHeader]],
  // Test Access-Control-Request-Headers is sorted https://crbug.com/452391
  [OTHER_BASE_URL + 'mode=cors&method=GET&headers=CUSTOM2&ACAOrigin=*&PACAOrigin=*&PACAHeaders=x-servicEworker-u, x-servicEworker-ua, x-servicewOrker-test, x-sErviceworker-s, x-sErviceworker-v&PreflightTest=200&PACRMethod=GET&PACRHeaders=x-serviceworker-s, x-serviceworker-test, x-serviceworker-u, x-serviceworker-ua, x-serviceworker-v',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, hasCustomHeader2]],

  // method=POST

  // CORS check
  // https://fetch.spec.whatwg.org/#concept-cors-check
  // Tests for Access-Control-Allow-Origin header.
  [OTHER_BASE_URL + 'mode=cors&method=POST',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=POST&ACAOrigin=*',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPOST]],
  [OTHER_BASE_URL + 'mode=cors&method=POST&ACAOrigin=' + BASE_ORIGIN,
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPOST]],
  [OTHER_BASE_URL + 'mode=cors&method=POST&ACAOrigin=' + BASE_ORIGIN +
   ',http://www.example.com',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=POST&ACAOrigin=http://www.example.com',
   [fetchRejected]],

  // CORS filtered response
  // https://fetch.spec.whatwg.org/#concept-filtered-response-cors
  // Tests for Access-Control-Expose-Headers header.
  [OTHER_BASE_URL + 'mode=cors&method=POST&ACAOrigin=*&ACEHeaders=X-ServiceWorker-ServerHeader',
   [fetchResolved, noContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsPOST]],
  [OTHER_BASE_URL + 'mode=cors&method=POST&ACAOrigin=' + BASE_ORIGIN +
   '&ACEHeaders=X-ServiceWorker-ServerHeader',
   [fetchResolved, noContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsPOST]],
  [OTHER_BASE_URL + 'mode=cors&method=POST&ACAOrigin=*&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsPOST]],
  [OTHER_BASE_URL + 'mode=cors&method=POST&ACAOrigin=' + BASE_ORIGIN +
   '&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsPOST]],

  // CORS preflight fetch
  // https://fetch.spec.whatwg.org/#cors-preflight-fetch
  // Tests for Access-Control-Allow-Headers header.
  [OTHER_BASE_URL + 'mode=cors&method=POST&headers=CUSTOM',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=POST&headers=CUSTOM&ACAOrigin=*',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=POST&headers=CUSTOM&ACAOrigin=*&ACAHeaders=x-serviceworker-test',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPOST, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=POST&headers=CUSTOM&ACAOrigin=*&ACAHeaders=x-serviceworker-test&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsPOST, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=POST&headers=CUSTOM&ACAOrigin=' +
   BASE_ORIGIN,
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=POST&headers=CUSTOM&ACAOrigin=' +
   BASE_ORIGIN + '&ACAHeaders=x-serviceworker-test',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPOST, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=POST&headers=CUSTOM&ACAOrigin=' +
   BASE_ORIGIN +
   '&ACAHeaders=x-serviceworker-test&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsPOST, hasCustomHeader]],

  // Test that Access-Control-Allow-Headers is checked in CORS preflight fetch.
  [OTHER_BASE_URL + 'mode=cors&method=POST&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAHeaders=x-serviceworker-test&PreflightTest=200',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPOST, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=POST&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&ACAHeaders=x-serviceworker-test&PreflightTest=200',
   [fetchRejected]],

  // Test that CORS check is done in both preflight and main fetch.
  [OTHER_BASE_URL + 'mode=cors&method=POST&headers=CUSTOM&ACAOrigin=*&PACAHeaders=x-serviceworker-test&PreflightTest=200',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=POST&headers=CUSTOM&PACAOrigin=*&PACAHeaders=x-serviceworker-test&PreflightTest=200',
   [fetchRejected]],

  // Test that Access-Control-Expose-Headers of CORS preflight is ignored.
  [OTHER_BASE_URL + 'mode=cors&method=POST&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAHeaders=x-serviceworker-test&PACEHeaders=Content-Length, X-ServiceWorker-ServerHeader&PreflightTest=200',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPOST, hasCustomHeader]],

  // Test that CORS preflight with Status 2XX succeeds.
  [OTHER_BASE_URL + 'mode=cors&method=POST&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAHeaders=x-serviceworker-test&PreflightTest=201',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPOST, hasCustomHeader]],

  // Test that CORS preflight with Status other than 2XX fails.
  // https://crbug.com/452394
  [OTHER_BASE_URL + 'mode=cors&method=POST&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAHeaders=x-serviceworker-test&PreflightTest=301',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=POST&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAHeaders=x-serviceworker-test&PreflightTest=401',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=POST&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAHeaders=x-serviceworker-test&PreflightTest=500',
   [fetchRejected]],

  // Test CORS preflight with multiple request headers.
  [OTHER_BASE_URL + 'mode=cors&method=POST&headers=CUSTOM2&ACAOrigin=*&PACAOrigin=*&PACAHeaders=x-servicEworker-u, x-servicEworker-ua, x-servicewOrker-test, x-sErviceworker-s, x-sErviceworker-v&PreflightTest=200',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPOST, hasCustomHeader2]],
  [OTHER_BASE_URL + 'mode=cors&method=POST&headers=CUSTOM2&ACAOrigin=*&PACAOrigin=*&PACAHeaders=x-servicewOrker-test&PreflightTest=200',
   [fetchRejected]],

  // Test request headers sent in CORS preflight requests.
  [OTHER_BASE_URL + 'mode=cors&method=POST&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAHeaders=x-serviceworker-test&PreflightTest=200&PACRMethod=POST&PACRHeaders=x-serviceworker-test',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPOST, hasCustomHeader]],
  // Test Access-Control-Request-Headers is sorted https://crbug.com/452391
  [OTHER_BASE_URL + 'mode=cors&method=POST&headers=CUSTOM2&ACAOrigin=*&PACAOrigin=*&PACAHeaders=x-servicEworker-u, x-servicEworker-ua, x-servicewOrker-test, x-sErviceworker-s, x-sErviceworker-v&PreflightTest=200&PACRMethod=POST&PACRHeaders=x-serviceworker-s, x-serviceworker-test, x-serviceworker-u, x-serviceworker-ua, x-serviceworker-v',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPOST, hasCustomHeader2]],

  // method=PUT

  // CORS check
  // https://fetch.spec.whatwg.org/#concept-cors-check
  // Tests for Access-Control-Allow-Origin header.
  // CORS preflight fetch
  // https://fetch.spec.whatwg.org/#cors-preflight-fetch
  // Tests for Access-Control-Allow-Methods header.
  // Tests for Access-Control-Allow-Headers header.
  [OTHER_BASE_URL + 'mode=cors&method=PUT',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAMethods=PUT',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=*',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=*&ACAMethods=PUT',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPUT]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=*&headers=CUSTOM&ACAMethods=PUT',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=*&headers=CUSTOM&ACAMethods=PUT&ACAHeaders=x-serviceworker-test',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPUT, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=*&headers=CUSTOM&ACAMethods=PUT&ACAHeaders=x-serviceworker-test&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsPUT, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=*&headers=CUSTOM&ACAMethods=PUT, XXX',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=*&headers=CUSTOM&ACAMethods=PUT, XXX&ACAHeaders=x-serviceworker-test',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPUT, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=*&headers=CUSTOM&ACAMethods=PUT, XXX&ACAHeaders=x-serviceworker-test&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsPUT, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=' + BASE_ORIGIN,
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=' + BASE_ORIGIN +
   '&ACAMethods=PUT',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPUT]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=' + BASE_ORIGIN +
   '&headers=CUSTOM&ACAMethods=PUT',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=' + BASE_ORIGIN +
   '&headers=CUSTOM&ACAMethods=PUT&ACAHeaders=x-serviceworker-test',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPUT, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=' + BASE_ORIGIN +
   '&headers=CUSTOM&ACAMethods=PUT&ACAHeaders=x-serviceworker-test&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsPUT, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=' + BASE_ORIGIN +
   '&headers=CUSTOM&ACAMethods=PUT, XXX',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=' + BASE_ORIGIN +
   '&headers=CUSTOM&ACAMethods=PUT, XXX&ACAHeaders=x-serviceworker-test',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPUT, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=' + BASE_ORIGIN +
   '&headers=CUSTOM&ACAMethods=PUT, XXX&ACAHeaders=x-serviceworker-test&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsPUT, hasCustomHeader]],

  // Test that Access-Control-Allow-Methods is checked in CORS preflight fetch.
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=*&PACAOrigin=*&PACAMethods=PUT&PreflightTest=200',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPUT]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=*&PACAOrigin=*&ACAMethods=PUT&PreflightTest=200',
   [fetchRejected]],

  // Test that Access-Control-Allow-Headers is checked in CORS preflight fetch.
  [OTHER_BASE_URL + 'mode=cors&method=PUT&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAMethods=PUT&PACAHeaders=x-serviceworker-test&PreflightTest=200',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPUT, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAMethods=PUT&ACAHeaders=x-serviceworker-test&PreflightTest=200',
   [fetchRejected]],

  // Test that CORS check is done in both preflight and main fetch.
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=*&PACAMethods=PUT&PreflightTest=200',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&PACAOrigin=*&PACAMethods=PUT&PreflightTest=200',
   [fetchRejected]],

  // Test that Access-Control-Expose-Headers of CORS preflight is ignored.
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=*&PACAOrigin=*&PACAMethods=PUT&PACEHeaders=Content-Length, X-ServiceWorker-ServerHeader&PreflightTest=200',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPUT]],

  // Test that CORS preflight with Status 2XX succeeds.
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=*&PACAOrigin=*&PACAMethods=PUT&PreflightTest=201',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPUT]],

  // Test that CORS preflight with Status other than 2XX fails.
  // https://crbug.com/452394
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=*&PACAOrigin=*&PACAMethods=PUT&PreflightTest=301',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=*&PACAOrigin=*&PACAMethods=PUT&PreflightTest=401',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&ACAOrigin=*&PACAOrigin=*&PACAMethods=PUT&PreflightTest=500',
   [fetchRejected]],

  // Test CORS preflight with multiple request headers.
  [OTHER_BASE_URL + 'mode=cors&method=PUT&headers=CUSTOM2&ACAOrigin=*&PACAOrigin=*&PACAMethods=PUT&PACAHeaders=x-servicEworker-u, x-servicEworker-ua, x-servicewOrker-test, x-sErviceworker-s, x-sErviceworker-v&PreflightTest=200',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPUT, hasCustomHeader2]],
  [OTHER_BASE_URL + 'mode=cors&method=PUT&headers=CUSTOM2&ACAOrigin=*&PACAOrigin=*&PACAMethods=PUT&PACAHeaders=x-servicewOrker-test&PreflightTest=200',
   [fetchRejected]],

  // Test request headers sent in CORS preflight requests.
  [OTHER_BASE_URL + 'mode=cors&method=PUT&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAMethods=PUT&PACAHeaders=x-serviceworker-test&PreflightTest=200&PACRMethod=PUT&PACRHeaders=x-serviceworker-test',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPUT, hasCustomHeader]],
  // Test Access-Control-Request-Headers is sorted https://crbug.com/452391
  [OTHER_BASE_URL + 'mode=cors&method=PUT&headers=CUSTOM2&ACAOrigin=*&PACAOrigin=*&PACAMethods=PUT&PACAHeaders=x-servicEworker-u, x-servicEworker-ua, x-servicewOrker-test, x-sErviceworker-s, x-sErviceworker-v&PreflightTest=200&PACRMethod=PUT&PACRHeaders=x-serviceworker-s, x-serviceworker-test, x-serviceworker-u, x-serviceworker-ua, x-serviceworker-v',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPUT, hasCustomHeader2]],

  // method=XXX

  // CORS check
  // https://fetch.spec.whatwg.org/#concept-cors-check
  // Tests for Access-Control-Allow-Origin header.
  // CORS preflight fetch
  // https://fetch.spec.whatwg.org/#cors-preflight-fetch
  // Tests for Access-Control-Allow-Methods header.
  // Tests for Access-Control-Allow-Headers header.

  [OTHER_BASE_URL + 'mode=cors&method=XXX',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAMethods=XXX',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=*',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=*&ACAMethods=XXX',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsXXX]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=*&headers=CUSTOM&ACAMethods=XXX',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=*&headers=CUSTOM&ACAMethods=XXX&ACAHeaders=x-serviceworker-test',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsXXX, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=*&headers=CUSTOM&ACAMethods=XXX&ACAHeaders=x-serviceworker-test&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsXXX, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=*&headers=CUSTOM&ACAMethods=PUT, XXX',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=*&headers=CUSTOM&ACAMethods=PUT, XXX&ACAHeaders=x-serviceworker-test',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsXXX, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=*&headers=CUSTOM&ACAMethods=PUT, XXX&ACAHeaders=x-serviceworker-test&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsXXX, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=' + BASE_ORIGIN,
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=' + BASE_ORIGIN +
   '&ACAMethods=XXX',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsXXX]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=' + BASE_ORIGIN +
   '&headers=CUSTOM&ACAMethods=XXX',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=' + BASE_ORIGIN +
   '&headers=CUSTOM&ACAMethods=XXX&ACAHeaders=x-serviceworker-test',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsXXX, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=' + BASE_ORIGIN +
   '&headers=CUSTOM&ACAMethods=XXX&ACAHeaders=x-serviceworker-test&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsXXX, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=' + BASE_ORIGIN +
   '&headers=CUSTOM&ACAMethods=PUT, XXX',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=' + BASE_ORIGIN +
   '&headers=CUSTOM&ACAMethods=PUT, XXX&ACAHeaders=x-serviceworker-test',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsXXX, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=' + BASE_ORIGIN +
   '&headers=CUSTOM&ACAMethods=PUT, XXX&ACAHeaders=x-serviceworker-test&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsXXX, hasCustomHeader]],

  // Test that Access-Control-Allow-Methods is checked in CORS preflight fetch.
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=*&PACAOrigin=*&PACAMethods=XXX&PreflightTest=200',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsXXX]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=*&PACAOrigin=*&ACAMethods=XXX&PreflightTest=200',
   [fetchRejected]],

  // Test that Access-Control-Allow-Headers is checked in CORS preflight fetch.
  [OTHER_BASE_URL + 'mode=cors&method=XXX&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAMethods=XXX&PACAHeaders=x-serviceworker-test&PreflightTest=200',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsXXX, hasCustomHeader]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAMethods=XXX&ACAHeaders=x-serviceworker-test&PreflightTest=200',
   [fetchRejected]],

  // Test that CORS check is done in both preflight and main fetch.
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=*&PACAMethods=XXX&PreflightTest=200',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&PACAOrigin=*&PACAMethods=XXX&PreflightTest=200',
   [fetchRejected]],

  // Test that Access-Control-Expose-Headers of CORS preflight is ignored.
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=*&PACAOrigin=*&PACAMethods=XXX&PACEHeaders=Content-Length, X-ServiceWorker-ServerHeader&PreflightTest=200',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsXXX]],

  // Test that CORS preflight with Status 2XX succeeds.
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=*&PACAOrigin=*&PACAMethods=XXX&PreflightTest=201',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsXXX]],

  // Test that CORS preflight with Status other than 2XX fails.
  // https://crbug.com/452394
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=*&PACAOrigin=*&PACAMethods=XXX&PreflightTest=301',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=*&PACAOrigin=*&PACAMethods=XXX&PreflightTest=401',
   [fetchRejected]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&ACAOrigin=*&PACAOrigin=*&PACAMethods=XXX&PreflightTest=500',
   [fetchRejected]],

  // Test CORS preflight with multiple request headers.
  [OTHER_BASE_URL + 'mode=cors&method=XXX&headers=CUSTOM2&ACAOrigin=*&PACAOrigin=*&PACAMethods=XXX&PACAHeaders=x-servicEworker-u, x-servicEworker-ua, x-servicewOrker-test, x-sErviceworker-s, x-sErviceworker-v&PreflightTest=200',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsXXX, hasCustomHeader2]],
  [OTHER_BASE_URL + 'mode=cors&method=XXX&headers=CUSTOM2&ACAOrigin=*&PACAOrigin=*&PACAMethods=XXX&PACAHeaders=x-servicewOrker-test&PreflightTest=200',
   [fetchRejected]],

  // Test request headers sent in CORS preflight requests.
  [OTHER_BASE_URL + 'mode=cors&method=XXX&headers=CUSTOM&ACAOrigin=*&PACAOrigin=*&PACAMethods=XXX&PACAHeaders=x-serviceworker-test&PreflightTest=200&PACRMethod=XXX&PACRHeaders=x-serviceworker-test',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsXXX, hasCustomHeader]],
  // Test Access-Control-Request-Headers is sorted https://crbug.com/452391
  [OTHER_BASE_URL + 'mode=cors&method=XXX&headers=CUSTOM2&ACAOrigin=*&PACAOrigin=*&PACAMethods=XXX&PACAHeaders=x-servicEworker-u, x-servicEworker-ua, x-servicewOrker-test, x-sErviceworker-s, x-sErviceworker-v&PreflightTest=200&PACRMethod=XXX&PACRHeaders=x-serviceworker-s, x-serviceworker-test, x-serviceworker-u, x-serviceworker-ua, x-serviceworker-v',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsXXX, hasCustomHeader2]],
];

if (self.importScripts) {
  executeTests(TEST_TARGETS);
  done();
}
