<?php
header("Content-Security-Policy: suborigin foo; suborigin bar");
?>
<!DOCTYPE html>
<html>
<body>
    <script>
    if (window.testRunner)
        testRunner.dumpAsText();
    </script>
</body>
</html>
