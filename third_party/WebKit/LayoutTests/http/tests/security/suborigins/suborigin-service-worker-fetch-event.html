<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Service Worker cannot access fetch events from a suborigin</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>

<body>
<script>

var t_nonsuborigin = async_test(
  'Service worker can access fetch events from a non-suborigin on the origin');
var t_suborigin = async_test(
  'Service worker cannot access fetch events from a suborigin on the origin');
window.addEventListener('message', function(e) {
    if (e.data.id == 'suborigin') {
      t_suborigin.step(function() {
          assert_equals(e.data.type, 'response');
          t_suborigin.done();
        });
    } else if (e.data.id == 'nonsuborigin') {
      t_nonsuborigin.step(function() {
          assert_equals(e.data.type, 'error');
          t_nonsuborigin.done();
        });
    } else {
      assert_unreached('id must be one of \'suborigin\' or \'nonsuborigin\'');
    }
  });

navigator.serviceWorker.register(
    '/security/suborigins/resources/sw-reject-all-with-error.js')
  .then(function(swr) {
      var tests_started = false;

      swr.installing.onstatechange = function() {
        if (tests_started)
          return;

        tests_started = true;

        var iframe1 = document.createElement('iframe');
        iframe1.src = 'resources/fetch-resource.php?suborigin=foobar&' +
                      'resource=/resources/dummy.txt&id=suborigin&bypass';
        document.body.appendChild(iframe1);

        var iframe2 = document.createElement('iframe');
        iframe2.src = 'resources/fetch-resource.php?' +
                      'resource=/resources/dummy.txt&id=nonsuborigin&bypass';
        document.body.appendChild(iframe2);
      };
    });
</script>
</body>
