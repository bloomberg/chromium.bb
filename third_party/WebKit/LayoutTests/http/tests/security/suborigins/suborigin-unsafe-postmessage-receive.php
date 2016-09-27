<?php
header("Suborigin: foobar 'unsafe-postmessage-receive'");
?>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Validate that unsafe-postmessage-receive allows Suborigin to receive messages as physical origin via postMessage.</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<script>
async_test(t => {
    window.addEventListener('message', make_test(t, 'iframe'));
  },
  'Validate serialization of event.origin and event.suborigin in ' +
  'postMessage from an iframe using \'unsafe-postmessage-receive\'');
async_test(t => {
    window.addEventListener('message', make_test(t, 'window'));
  },
  'Validate serialization of event.origin and event.suborigin in ' +
  'postMessage from a window \'unsafe-postmessage-receive\'');

function make_test(t, name) {
  return t.step_func(function(event) {
      if (event.data.type === name) {
        assert_equals(event.origin, 'http-so://' + name + '.127.0.0.1:8000');
        assert_equals(event.suborigin, name);
        assert_equals(event.data.suborigin,  name);
        t.done();
      }
    });
}

window.open(
  'resources/post-document-to-parent.php?suborigin=window&type=window&' +
  'target=http://127.0.0.1:8000');
</script>
<iframe src="resources/post-document-to-parent.php?suborigin=iframe&type=iframe&target=http://127.0.0.1:8000"></iframe>
</html>
