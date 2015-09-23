<?php
if ($_GET["suborigin"]) {
    header("Content-Security-Policy: suborigin ". $_GET["suborigin"]);
}
?>
<!DOCTYPE html>
<html>
<script>
window.parent.postMessage(document.suborigin, '*');
</script>
</html>
