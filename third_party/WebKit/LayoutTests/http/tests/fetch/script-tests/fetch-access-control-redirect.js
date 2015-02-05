if (self.importScripts) {
  importScripts('../resources/fetch-test-helpers.js');
  importScripts('/serviceworker/resources/fetch-access-control-util.js');
}

var TEST_TARGETS = [
  // Redirect: same origin -> same origin
  [REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=same-origin&method=GET',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, authCheck1]],

  // https://fetch.spec.whatwg.org/#concept-http-fetch
  // Step 4, Case 301/302/303/307/308:
  // Step 2: If location is null, return response.
  [REDIRECT_URL + 'noLocation' +
   '&mode=same-origin&method=GET&NoRedirectTest=true',
   [fetchResolved, hasBody, typeBasic],
   [checkJsonpNoRedirect]],
  // Step 5: If locationURL is failure, return a network error.
  [REDIRECT_URL + 'http://' +
   '&mode=same-origin&method=GET',
   [fetchRejected]],

  [REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=same-origin&method=GET&headers=CUSTOM',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, hasCustomHeader, authCheck1]],
  // Chrome changes the method from POST to GET when it recieves 301 redirect
  // response. See a note in http://tools.ietf.org/html/rfc7231#section-6.4.2
  [REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=same-origin&method=POST&Status=301',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, authCheck1]],
  // Chrome changes the method from POST to GET when it recieves 302 redirect
  // response. See a note in http://tools.ietf.org/html/rfc7231#section-6.4.3
  [REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=same-origin&method=POST',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, authCheck1]],
  // GET method must be used for 303 redirect.
  [REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=same-origin&method=POST&Status=303',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, authCheck1]],
  // The 307 redirect response doesn't change the method.
  [REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=same-origin&method=POST&Status=307',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsPOST, authCheck1]],
  // The 308 redirect response doesn't change the method.
  // FIXME: currently this and following 308 tests are disabled because they
  // fail on try bots, probably due to Apache/PHP versions.
  // https://crbug.com/451938
  // [REDIRECT_URL + encodeURIComponent(BASE_URL) +
  //  '&mode=same-origin&method=POST&Status=308',
  //  [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
  //  [methodIsPOST, authCheck1]],

  // Do not redirect for other status even if Location header exists.
  [REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=same-origin&method=POST&Status=201&NoRedirectTest=true',
   [fetchResolved, hasBody, typeBasic],
   [checkJsonpNoRedirect]],

  [REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=same-origin&method=PUT',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsPUT, authCheck1]],

  [REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=no-cors&method=GET&headers=CUSTOM',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, noCustomHeader, authCheck1]],

  [REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=cors&method=GET&headers=CUSTOM',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, hasCustomHeader, authCheck1]],

  // Credential test
  [REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=cors&credentials=omit&method=GET',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, authCheckNone]],
  [REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=cors&credentials=include&method=GET',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, authCheck1]],
  [REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=cors&credentials=same-origin&method=GET',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeBasic],
   [methodIsGET, authCheck1]],

  // Redirect: same origin -> other origin
  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL) +
   '&mode=same-origin&method=GET',
   [fetchRejected]],
  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL) +
   '&mode=same-origin&method=POST',
   [fetchRejected]],
  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL) +
   '&mode=same-origin&method=PUT',
   [fetchRejected]],

  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL) +
   '&mode=no-cors&method=GET&headers=CUSTOM',
   [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([methodIsGET, noCustomHeader, authCheck2])],

  // Status code tests for mode="no-cors"
  // The 301 redirect response changes POST method to GET method.
  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL) +
   '&mode=no-cors&method=POST&Status=301',
   [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([methodIsGET, authCheck2])],
  // The 302 redirect response changes POST method to GET method.
  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL) +
   '&mode=no-cors&method=POST',
   [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([methodIsGET, authCheck2])],
  // GET method must be used for 303 redirect.
  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL) +
   '&mode=no-cors&method=POST&Status=303',
   [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([methodIsGET, authCheck2])],
  // The 307 redirect response doesn't change the method.
  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL) +
   '&mode=no-cors&method=POST&Status=307',
   [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([methodIsPOST, authCheck2])],
  // The 308 redirect response doesn't change the method.
  // FIXME: disabled due to https://crbug.com/451938
  // [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL) +
  // '&mode=no-cors&method=POST&Status=308',
  // [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
  // onlyOnServiceWorkerProxiedTest([methodIsPOST, authCheck2])],

  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL) +
   '&mode=cors&method=GET',
   [fetchRejected]],
  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL) +
   '&mode=cors&method=PUT',
   [fetchRejected]],

  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL + '&ACAOrigin=*') +
   '&mode=cors&method=GET',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL + '&ACAOrigin=*') +
   '&mode=cors&method=PUT',
   [fetchRejected]],
  [REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL + '&ACAOrigin=*&ACAMethods=PUT') +
   '&mode=cors&method=PUT',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPUT, noCustomHeader, authCheckNone]],

  // Status code tests for mode="cors"
  // The 301 redirect response MAY change the request method from POST to GET.
  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL + '&ACAOrigin=*') +
   '&mode=cors&method=POST&Status=301',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET]],
  // The 302 redirect response MAY change the request method from POST to GET.
  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL + '&ACAOrigin=*') +
   '&mode=cors&method=POST&Status=302',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET]],
  // GET method must be used for 303 redirect.
  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL + '&ACAOrigin=*') +
   '&mode=cors&method=POST&Status=303',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET]],
  // The 307 redirect response MUST NOT change the method.
  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL + '&ACAOrigin=*') +
   '&mode=cors&method=POST&Status=307',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPOST]],
  // The 308 redirect response MUST NOT change the method.
  // FIXME: disabled due to https://crbug.com/451938
  // [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL + '&ACAOrigin=*') +
  //  '&mode=cors&method=POST&Status=308',
  //  [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
  //  [methodIsPOST]],

  // Server header
  [REDIRECT_URL +
   encodeURIComponent(
     OTHER_BASE_URL +
     '&ACAOrigin=' + BASE_ORIGIN +
     '&ACEHeaders=Content-Length, X-ServiceWorker-ServerHeader') +
   '&mode=cors&method=GET',
   [fetchResolved, hasContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],

  // Credential test
  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL) +
   '&mode=cors&credentials=omit&method=GET',
   [fetchRejected]],
  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL) +
   '&mode=cors&credentials=include&method=GET',
   [fetchRejected]],
  [REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL) +
   '&mode=cors&credentials=same-origin&method=GET',
   [fetchRejected]],

  [REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL + '&ACAOrigin=' + BASE_ORIGIN + '') +
   '&mode=cors&credentials=omit&method=GET',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  [REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL +
                      '&ACAOrigin=' + BASE_ORIGIN + '&ACACredentials=true') +
   '&mode=cors&credentials=omit&method=GET',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],

  [REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL + '&ACAOrigin=' + BASE_ORIGIN + '') +
   '&mode=cors&credentials=include&method=GET',
   [fetchRejected]],
  [REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL +
                      '&ACAOrigin=' + BASE_ORIGIN + '&ACACredentials=true') +
   '&mode=cors&credentials=include&method=GET',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheck2]],

  [REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL + '&ACAOrigin=' + BASE_ORIGIN + '') +
   '&mode=cors&credentials=same-origin&method=GET',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  [REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL +
                      '&ACAOrigin=' + BASE_ORIGIN + '&ACACredentials=true') +
   '&mode=cors&credentials=same-origin&method=GET',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],

  // Redirect: other origin -> same origin
  [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=same-origin&method=GET',
   [fetchRejected]],
  [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=same-origin&method=POST',
   [fetchRejected]],

  [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=no-cors&method=GET',
   [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([methodIsGET, authCheck1])],
  [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=no-cors&method=GET&headers=CUSTOM',
   [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([methodIsGET, noCustomHeader, authCheck1])],

  // Status code tests for mode="no-cors"
  // The 301 redirect response MAY change the request method from POST to GET.
  [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=no-cors&method=POST&Status=301',
   [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([methodIsGET, authCheck1])],
  // The 302 redirect response MAY change the request method from POST to GET.
  [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=no-cors&method=POST',
   [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([methodIsGET, authCheck1])],
  // GET method must be used for 303 redirect.
  [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=no-cors&method=POST&Status=303',
   [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([methodIsGET, authCheck1])],
  // The 307 redirect response MUST NOT change the method.
  [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=no-cors&method=POST&Status=307',
   [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([methodIsPOST, authCheck1])],
  // The 308 redirect response MUST NOT change the method.
  // FIXME: disabled due to https://crbug.com/451938
  // [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL) +
  //  '&mode=no-cors&method=POST&Status=308',
  //  [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
  //  onlyOnServiceWorkerProxiedTest([methodIsPOST, authCheck1])],

  [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=cors&method=GET',
   [fetchRejected]],
  [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL) +
   '&mode=cors&method=GET&ACAOrigin=*',
   [fetchRejected]],
  [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL + 'ACAOrigin=*') +
   '&mode=cors&method=GET&ACAOrigin=*',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],

  // Status code tests for mode="cors"
  // The 301 redirect response MAY change the request method from POST to GET.
  [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL + 'ACAOrigin=*') +
   '&mode=cors&method=post&ACAOrigin=*&Status=301',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET]],
  // The 302 redirect response MAY change the request method from POST to GET.
  [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL + 'ACAOrigin=*') +
   '&mode=cors&method=post&ACAOrigin=*&Status=302',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET]],
  // GET method must be used for 303 redirect.
  [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL + 'ACAOrigin=*') +
   '&mode=cors&method=post&ACAOrigin=*&Status=303',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET]],
  // The 307 redirect response MUST NOT change the method.
  [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL + 'ACAOrigin=*') +
   '&mode=cors&method=post&ACAOrigin=*&Status=307',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPOST]],
  // The 308 redirect response MUST NOT change the method.
  // FIXME: disabled due to https://crbug.com/451938
  // [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL + 'ACAOrigin=*') +
  //  '&mode=cors&method=post&ACAOrigin=*&Status=308',
  //  [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
  //  [methodIsPOST]],

  // Once CORS preflight flag is set, redirecting to the cross-origin is not
  // allowed.
  // Custom method
  [OTHER_REDIRECT_URL +
   encodeURIComponent(BASE_URL + 'ACAOrigin=*&ACAMethods=PUT') +
   '&mode=cors&method=PUT&ACAOrigin=*&ACAMethods=PUT',
   [fetchRejected]],
  // Custom header
  [OTHER_REDIRECT_URL +
   encodeURIComponent(
       BASE_URL +
       'ACAOrigin=' + BASE_ORIGIN + '&ACAHeaders=x-serviceworker-test') +
   '&mode=cors&method=GET&headers=CUSTOM&ACAOrigin=*',
   [fetchRejected]],

  // Credentials test
  [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL + 'ACAOrigin=*') +
   '&mode=cors&credentials=omit&method=GET&ACAOrigin=*',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL + 'ACAOrigin=*') +
   '&mode=cors&credentials=include&method=GET&ACAOrigin=*',
   [fetchRejected]],
  [OTHER_REDIRECT_URL + encodeURIComponent(BASE_URL + 'ACAOrigin=*') +
   '&mode=cors&credentials=same-origin&method=GET&ACAOrigin=*',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  [OTHER_REDIRECT_URL +
   encodeURIComponent(BASE_URL + 'ACAOrigin=null&ACACredentials=true') +
   '&mode=cors&credentials=omit&method=GET' +
   '&ACAOrigin=' + BASE_ORIGIN + '&ACACredentials=true',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  [OTHER_REDIRECT_URL +
   encodeURIComponent(BASE_URL + 'ACAOrigin=null&ACACredentials=true') +
   '&mode=cors&credentials=include&method=GET' +
   '&ACAOrigin=' + BASE_ORIGIN + '&ACACredentials=true',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheck1]],
  [OTHER_REDIRECT_URL +
   encodeURIComponent(BASE_URL + 'ACAOrigin=null&ACACredentials=true') +
   '&mode=cors&credentials=same-origin&method=GET' +
   '&ACAOrigin=' + BASE_ORIGIN + '&ACACredentials=true',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],

  // Redirect: other origin -> other origin
  [OTHER_REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL) +
   '&mode=same-origin&method=GET',
   [fetchRejected]],
  [OTHER_REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL) +
   '&mode=no-cors&method=GET',
   [fetchResolved, noContentLength, noServerHeader, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([methodIsGET, authCheck2])],
  [OTHER_REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL) +
   '&mode=cors&method=GET',
   [fetchRejected]],
  [OTHER_REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL) +
   '&mode=cors&method=GET&ACAOrigin=*',
   [fetchRejected]],
  [OTHER_REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=*') +
   '&mode=cors&method=GET&ACAOrigin=*',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  [OTHER_REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=' + BASE_ORIGIN + '') +
   '&mode=cors&method=GET&ACAOrigin=*',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  [OTHER_REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=*') +
   '&mode=cors&method=GET&ACAOrigin=' + BASE_ORIGIN + '',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  [OTHER_REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=' + BASE_ORIGIN + '') +
   '&mode=cors&method=GET&ACAOrigin=' + BASE_ORIGIN + '',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],


  // Status code tests for mode="cors"
  // The 301 redirect response MAY change the request method from POST to GET.
  [OTHER_REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=*') +
   '&mode=cors&method=POST&ACAOrigin=*&Status=301',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET]],
  // The 302 redirect response MAY change the request method from POST to GET.
  [OTHER_REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=*') +
   '&mode=cors&method=POST&ACAOrigin=*&Status=302',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET]],
  // GET method must be used for 303 redirect.
  [OTHER_REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=*') +
   '&mode=cors&method=POST&ACAOrigin=*&Status=303',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET]],
  // The 307 redirect response MUST NOT change the method.
  [OTHER_REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=*') +
   '&mode=cors&method=POST&ACAOrigin=*&Status=307',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsPOST]],
  // The 308 redirect response MUST NOT change the method.
  // FIXME: disabled due to https://crbug.com/451938
  // [OTHER_REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=*') +
  //  '&mode=cors&method=POST&ACAOrigin=*&Status=308',
  //  [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
  //  [methodIsPOST]],

  // Server header
  [OTHER_REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL +
                      'ACAOrigin=*&ACEHeaders=X-ServiceWorker-ServerHeader') +
   '&mode=cors&method=GET&ACAOrigin=*',
   [fetchResolved, noContentLength, hasServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],

  // Once CORS preflight flag is set, redirecting to the cross-origin is not
  // allowed.
  // Custom method
  [OTHER_REDIRECT_URL +
   encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=*&ACAMethods=PUT') +
   '&mode=cors&method=PUT&ACAOrigin=*&ACAMethods=PUT',
   [fetchRejected]],
  // Custom header
  [OTHER_REDIRECT_URL +
   encodeURIComponent(
     OTHER_BASE_URL +
     'ACAOrigin=' + BASE_ORIGIN + '&ACAHeaders=x-serviceworker-test') +
   '&mode=cors&method=GET&headers=CUSTOM' +
   '&ACAOrigin=' + BASE_ORIGIN + '&ACAHeaders=x-serviceworker-test',
   [fetchRejected]],

  // Redirect loop: same origin -> same origin
  [REDIRECT_LOOP_URL + encodeURIComponent(BASE_URL) + '&Count=20',
   [fetchResolved, hasContentLength, hasBody, typeBasic],
   [methodIsGET, authCheck1]],
  [REDIRECT_LOOP_URL + encodeURIComponent(BASE_URL) + '&Count=21',
   [fetchRejected]],

  // Redirect loop: same origin -> other origin
  [REDIRECT_LOOP_URL + encodeURIComponent(OTHER_BASE_URL + '&ACAOrigin=*') +
   '&Count=20&mode=cors&method=GET',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  // FIXME: due to the current implementation of Chromium,
  // Count=21 is resolved, Count=22 is rejected.
  // https://crbug.com/353768
  [REDIRECT_LOOP_URL + encodeURIComponent(OTHER_BASE_URL + '&ACAOrigin=*') +
   '&Count=22&mode=cors&method=GET',
   [fetchRejected]],

  // Redirect loop: other origin -> same origin
  [OTHER_REDIRECT_LOOP_URL + encodeURIComponent(BASE_URL + 'ACAOrigin=*') +
   '&Count=20&mode=cors&method=GET&ACAOrigin=*',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  [OTHER_REDIRECT_LOOP_URL + encodeURIComponent(BASE_URL + 'ACAOrigin=*') +
   '&Count=21&mode=cors&method=GET&ACAOrigin=*',
   [fetchRejected]],

  // Redirect loop: other origin -> other origin
  [OTHER_REDIRECT_LOOP_URL +
   encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=*') +
   '&Count=20&mode=cors&method=GET&ACAOrigin=*',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  [OTHER_REDIRECT_LOOP_URL +
   encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=*') +
   '&Count=21&mode=cors&method=GET&ACAOrigin=*',
   [fetchRejected]],
];

if (self.importScripts) {
  executeTests(TEST_TARGETS);
  done();
}
