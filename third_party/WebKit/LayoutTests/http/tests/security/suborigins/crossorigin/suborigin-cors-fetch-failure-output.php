<?php
header("Suborigin: foobar");
?>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Fetches from suborigins require responses with valid Access-Control-Allow-Suborigin header</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<body>
<script>
async_test(t => {
    var headers = new Headers();
    headers.append("x-custom-header", "foobar");
    var options = {
      headers: headers
    };
    fetch(
        "http://127.0.0.1:8000/security/resources/cors-script.php?cors=false",
        options)
      .then(t.unreached_func('Fetch succeeded'))
      .catch(t.step_func_done());
  }, 'Custom headers causes preflight failure');

async_test(t => {
      fetch(
        "http://127.0.0.1:8000/security/resources/cors-script.php?cors=false")
      .then(t.unreached_func('Fetch succeeded'))
      .catch(t.step_func_done());
  }, 'Lack of Access-Control-Allow-Suborigin on response causes failure');
</script>
</body>
</html>
