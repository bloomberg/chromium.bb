<?php
  if ($_SERVER['REQUEST_METHOD'] == 'POST') {
      header ("HTTP/1.1 302");
      header("Location: ./navigation-redirect-body.php?redirect");
  } else {
      echo urlencode($_SERVER['REQUEST_URI']);
  }
?>
