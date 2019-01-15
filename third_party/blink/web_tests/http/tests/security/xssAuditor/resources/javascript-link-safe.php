<?php header("X-XSS-Protection: 1"); ?>
<!DOCTYPE html>
<html>
<head>
</head>
<body>
<a id="anchorLink" href="javascript:alert('This is a safe script.')">test</a>
<script>
    var event = document.createEvent('MouseEvent');
    event.initEvent('click', true, true);
    document.getElementById('anchorLink').dispatchEvent(event);

    setTimeout(function() {
        if (window.testRunner)
            testRunner.notifyDone();
    }, 0);
</script>
</body>
</html>
