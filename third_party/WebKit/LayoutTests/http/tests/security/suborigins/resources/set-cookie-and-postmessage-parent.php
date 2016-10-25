<?php
setcookie($_GET["name"], $_GET["value"], 0, "/");
?>
<!DOCTYPE html>
<html>
<script>
window.parent.postMessage('set', '*');
</script>
</html>
