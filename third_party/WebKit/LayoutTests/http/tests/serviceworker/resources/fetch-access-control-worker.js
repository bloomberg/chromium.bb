importScripts('fetch-test-options.js');
importScripts('fetch-access-control-util.js');

var port = undefined;
var isTestTargetFetch = false;

self.onmessage = function(e) {
  var message = e.data;
  if ('port' in message) {
    port = message.port;
  } else if (message.msg === 'START TEST CASE') {
    isTestTargetFetch = true;
    port.postMessage({msg: 'READY'});
  }
};

self.addEventListener('fetch', function(event) {
    var originalURL = event.request.url;
    if (!isTestTargetFetch) {
      // Don't handle the event when it is not the test target fetch such as a
      // redirected fetch or for the iframe html.
      return;
    }
    isTestTargetFetch = false;
    var params = getQueryParams(originalURL);
    var init = getRequestInit(params);
    var url = params['url'];
    if (params['ignore']) {
      port.postMessage({fetchResult: 'ignored'});
      return;
    }
    event.respondWith(new Promise(function(resolve, reject) {
        try {
          var request = event.request;
          if (url) {
            request = new Request(url, init);
          } else if (!params['noChange']) {
            request = new Request(request, init);
          }
          var response;
          fetch(request)
            .then(function(res) {
                response = res;
                res.clone().text()
                  .then(function(body) {
                      // Send the result to fetch-access-control.html.
                      port.postMessage(
                        {
                          // Setting a comment will fail the test and show the
                          // comment in the result. Use this for debugging
                          // tests.
                          comment: undefined,

                          fetchResult: 'resolved',
                          body: body,
                          headers: headersToArray(response.headers),
                          type: response.type,
                          originalURL: originalURL
                        });
                      resolve(response);
                    })
                  .catch(function(e) {
                      // Send the result to fetch-access-control.html.
                      port.postMessage({fetchResult: 'error'});
                      reject();
                    });
              })
            .catch(function(e) {
                // Send the result to fetch-access-control.html.
                port.postMessage({fetchResult: 'rejected'});
                reject();
              });
        } catch (e) {
          // Send the result to fetch-access-control.html.
          port.postMessage({fetchResult: 'error'});
          reject();
        }
      }));
  });
