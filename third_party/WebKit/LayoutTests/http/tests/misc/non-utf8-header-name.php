<?php
# This header with a special character should be meaningless to the browser,
# and not cause a crash. Note that some HTTP servers may refuse to serve this.
# See: a href="https://blog.tigertech.net/posts/apache-cve-2016-8743/
header('HTTP/1.1 200 OK');
header('Ã: text/html');
echo '<script>';
echo '   if (window.testRunner)';
echo '       testRunner.dumpAsText();';
echo '</script>';
echo '<p>Test for <a href="https://bugs.webkit.org/show_bug.cgi?id=96284">bug 96284</a>: Non UTF-8 HTTP headers do not cause a crash.</p>';
?>
