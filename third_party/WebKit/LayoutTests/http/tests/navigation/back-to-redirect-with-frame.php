<?php
if (isset($_COOKIE['back-to-redirect-with-frame'])) {
    header('Location: resources/pass-and-notify-done.html');
    exit;
}

header("Cache-Control: no-store");
header("Set-Cookie: back-to-redirect-with-frame = true;");
?>
<body>
<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.dumpBackForwardList();
    testRunner.waitUntilDone();
}

window.onload = setTimeout(function() {
    window.location = "resources/go-back.html"
}, 10);
</script>
<iframe src="about:blank"></iframe>
</body>
