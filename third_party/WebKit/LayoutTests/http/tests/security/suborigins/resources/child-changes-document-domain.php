<?php
if ($_GET["report-only"]) {
    header("Content-Security-Policy-Report-Only: suborigin " . $_GET["suborigin"]);
} else {
    header("Content-Security-Policy: suborigin " . $_GET["suborigin"]);
}
?>
<!DOCTYPE html>
<script>
document.domain = '127.0.0.1';
window.parent.secret = 'I am a secret';
parent.postMessage('Done', '*');
</script>
