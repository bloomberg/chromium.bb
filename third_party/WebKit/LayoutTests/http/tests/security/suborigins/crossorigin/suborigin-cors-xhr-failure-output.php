<?php
header("Suborigin: foobar");
?>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>XHRs from suborigins require responses with valid Access-Control-Allow-Suborigin header</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<body>
<script>
async_test(t => {
    var xhr = new XMLHttpRequest();
    xhr.onerror = t.step_func_done();
    xhr.onload = t.unreached_func('XHR succeeded');
    // with-preflight is attached to distinguish console error outputs.
    xhr.open('GET', 'http://127.0.0.1:8000/security/resources/' +
                    'cors-script.php?cors=false&with-preflight');
    xhr.setRequestHeader('x-custom-header', 'foobar');
    xhr.send();
  }, 'Custom headers causes preflight failure');

async_test(t => {
    var xhr = new XMLHttpRequest();
    xhr.onerror = t.step_func_done();
    xhr.onload = t.unreached_func();
    // without-preflight is attached to distinguish console error outputs.
    xhr.open('GET', 'http://127.0.0.1:8000/security/resources/' +
                    'cors-script.php?cors=false&without-preflight');
    xhr.send();
  }, 'Lack of Access-Control-Allow-Suborigin on response causes failure');
</script>
</body>
</html>
