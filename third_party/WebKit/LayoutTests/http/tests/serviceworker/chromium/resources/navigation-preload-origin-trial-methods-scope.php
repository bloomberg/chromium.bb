<?php
  if (isset($_SERVER["HTTP_SERVICE_WORKER_NAVIGATION_PRELOAD"])) {
    echo ("Service-Worker-Navigation-Preload header set: " .
          $_SERVER["HTTP_SERVICE_WORKER_NAVIGATION_PRELOAD"]);
  } else {
    echo ("no Service-Worker-Navigation-Preload header");
  }
?>
