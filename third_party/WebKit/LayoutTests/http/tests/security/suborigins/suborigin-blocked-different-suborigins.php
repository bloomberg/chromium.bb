<?php
header("Content-Security-Policy: suborigin foobar1");
?>
<!DOCTYPE html>
<html>
<head>
<title>Block a frame in one suborigin from accessing another suborigin</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<script>
window.onmessage = function() {
    var iframe = document.getElementById('iframe');
    var secret = '';
    try {
        secret = iframe.contentWindow.secret;
        assert_unreached();
        done();
    } catch(e) {
        assert_equals(secret, "", "The parent frame should not be able to get the secret value from the child iframe.");
        done();
    };
};
</script>
<iframe id="iframe" src="resources/childsuborigin.php?suborigin=foobar2"></iframe>
</html>
