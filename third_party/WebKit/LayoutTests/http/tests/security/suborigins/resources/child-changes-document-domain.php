<?php
if ($_GET["suborigin"]) {
    header("Content-Security-Policy: suborigin " . $_GET["suborigin"]);
}
?>
<!DOCTYPE html>
<script>
try {
    document.domain = '127.0.0.1';
    window.parent.secret = 'I am a secret';
} catch (e) {
    parent.postMessage('' + e, '*');
}

parent.postMessage('Done', '*');
</script>
