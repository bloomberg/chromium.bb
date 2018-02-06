<?php
header('Content-Type: application/http-exchange+cbor');
?>
<!DOCTYPE html>
<body>
<script>
window.addEventListener('message', (event) => {
  event.data.port.postMessage({location: document.location.href});
}, false);
</script>
hello<br>
world
</body>
