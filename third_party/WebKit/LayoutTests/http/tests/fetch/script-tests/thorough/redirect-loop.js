if (self.importScripts) {
  importScripts('/fetch/resources/fetch-test-helpers.js');
  importScripts('/fetch/resources/thorough-util.js');
}

var TEST_TARGETS = [
  // Redirect loop: same origin -> same origin
  [REDIRECT_LOOP_URL + encodeURIComponent(BASE_URL) + '&Count=20&mode=cors' +
   '&credentials=same-origin',
   [fetchResolved, hasContentLength, hasBody, typeBasic],
   [methodIsGET, authCheck1]],
  [REDIRECT_LOOP_URL + encodeURIComponent(BASE_URL) + '&Count=21&mode=cors' +
   '&credentials=same-origin',
   [fetchRejected]],

  // Redirect loop: same origin -> other origin
  [REDIRECT_LOOP_URL + encodeURIComponent(OTHER_BASE_URL + '&ACAOrigin=*') +
   '&Count=20&mode=cors&credentials=same-origin&method=GET',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  // FIXME: due to the current implementation of Chromium,
  // Count=21 is resolved, Count=22 is rejected.
  // https://crbug.com/353768
  [REDIRECT_LOOP_URL + encodeURIComponent(OTHER_BASE_URL + '&ACAOrigin=*') +
   '&Count=22&mode=cors&credentials=same-origin&method=GET',
   [fetchRejected]],

  // Redirect loop: other origin -> same origin
  [OTHER_REDIRECT_LOOP_URL + encodeURIComponent(BASE_URL + 'ACAOrigin=*') +
   '&Count=20&mode=cors&credentials=same-origin&method=GET&ACAOrigin=*',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  [OTHER_REDIRECT_LOOP_URL + encodeURIComponent(BASE_URL + 'ACAOrigin=*') +
   '&Count=21&mode=cors&credentials=same-origin&method=GET&ACAOrigin=*',
   [fetchRejected]],

  // Redirect loop: other origin -> other origin
  [OTHER_REDIRECT_LOOP_URL +
   encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=*') +
   '&Count=20&mode=cors&credentials=same-origin&method=GET&ACAOrigin=*',
   [fetchResolved, noContentLength, noServerHeader, hasBody, typeCors],
   [methodIsGET, authCheckNone]],
  [OTHER_REDIRECT_LOOP_URL +
   encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=*') +
   '&Count=21&mode=cors&credentials=same-origin&method=GET&ACAOrigin=*',
   [fetchRejected]],
];

if (self.importScripts) {
  executeTests(TEST_TARGETS);
  done();
}
