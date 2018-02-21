<?php
    // We need live headers, so make sure it's not cached.
    header('Cache-Control: no-cache, no-store, must-revalidate, max-age=0');
    header("Link: <http://www.example.com/downloads/white-paper.pdf>; rel=\"canonical\"");
    header("Link: <http://example.com>;rel=\"alternate\";hreflang=\"xx\"", false);
?>
<html>
<body>
Hello, world!
</body>
</body>
