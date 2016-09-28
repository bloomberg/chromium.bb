<?php
header("Suborigin: foo, bar");
?>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Only first of multiple suborigins assigned</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<body>
<script>
async_test(t => {
    window.secret = 'unchanged';
    var iframe = document.createElement('iframe');
    iframe.src = 'resources/post-to-parent.php?suborigin=foo';
    iframe.onload = t.step_func(function() {
        try {
          var secret = iframe.contentWindow.secret;
          t.done();
        } catch (e) {
          assert_unreached(
            'Unable to read secret from iframe in same suborigin');
        }
      });
    document.body.appendChild(iframe);
  }, '\'foo\' is the assigned suborigin');

async_test(t => {
    window.secret = 'unchanged';
    var iframe = document.createElement('iframe');
    iframe.src = 'resources/post-to-parent.php?suborigin=bar';
    iframe.onload = t.step_func(function() {
        try {
          var secret = iframe.contentWindow.secret;
          assert_unreached(
            'Successfully read secret from iframe in a suborigin');
        } catch (e) {
          t.done();
        }
      });
    document.body.appendChild(iframe);
  }, '\'bar\' is not a valid suborigin');

async_test(t => {
    window.secret = 'unchanged';
    var iframe = document.createElement('iframe');
    iframe.src = 'resources/post-to-parent.php';
    iframe.onload = t.step_func(function() {
        try {
          var secret = iframe.contentWindow.secret;
          assert_unreached('Able to read secret from iframe without suborigin');
        } catch (e) {
          t.done();
        }
      });
    document.body.appendChild(iframe);
  }, 'Cannot access frame without suborigin');
</script>
</body>
</html>
