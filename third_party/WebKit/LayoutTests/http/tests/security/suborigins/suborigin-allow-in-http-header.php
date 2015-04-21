<?php
header("Content-Security-Policy: suborigin foobar");
?>
<!DOCTYPE html>
<html>
<head>
<title>Allow suborigin in HTTP header</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<script>
window.onmessage = function() {
    var iframe = document.getElementById('iframe');
    var secret = '';
    try {
        secret = iframe.contentWindow.secret;
        assert_equals(secret, "I am a secret", "The parent frame should be able to get the secret value from the child iframe.");
        done();
    } catch(e) {
        assert_unreached();
        done();
    };
};
</script>
<iframe id="iframe" src="resources/childsuborigin.php?suborigin=foobar"></iframe>
</html>
