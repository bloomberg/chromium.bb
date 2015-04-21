<?php
header("Content-Security-Policy: suborigin foobar");
?>
<html>
<body>
<p>
Test that window.onerror won't reveal any potentially sensitive script content if the latter is loaded from a different subdomain after a redirect. The test passes if you don't see any data from the linked resource.
</p>
<div id="result"></div>
<script>
if (window.testRunner) {
  testRunner.waitUntilDone();
  testRunner.dumpAsText();
}

window.onerror = function(message, url, line) {
  document.getElementById("result").textContent = "window.onerror message: " + message + " at " + url + ": " + line;
  if (window.testRunner)
    testRunner.notifyDone();
  return true;
}
</script>
<script src="/security/resources/redir.php?url=http://127.0.0.1:8000/security/resources/cross-origin-script.txt">
</script>
</body>
</html>
