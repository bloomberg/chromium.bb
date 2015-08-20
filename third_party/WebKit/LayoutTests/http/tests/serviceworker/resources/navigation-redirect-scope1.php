<?php
if (isset($_GET['url'])) {
  header("HTTP/1.1 302");
  $url = $_GET['url'];
  header("Location: $url");
  exit;
}
?>
<!DOCTYPE html>
<script>
  window.parent.postMessage(
      {
        id: 'last_url',
        result: location.href
      },
      'http://127.0.0.1:8000');
</script>
