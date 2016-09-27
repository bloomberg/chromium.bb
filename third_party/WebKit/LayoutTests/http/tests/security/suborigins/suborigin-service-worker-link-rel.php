<?php
header("Suborigin: foobar");
?>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Service Worker link rel fails from a suborigin</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<script>
var link = document.createElement('link');
link.setAttribute('rel', 'serviceworker');
link.setAttribute('href', '/serviceworker/resources/empty-worker.js');
link.onload = function() {
  assert_unreached(
    'Service worker was successfully registered with a <link> element');
}
link.onerror = function() {
  done();
}
document.getElementsByTagName('head')[0].appendChild(link);
</script>
</html>
