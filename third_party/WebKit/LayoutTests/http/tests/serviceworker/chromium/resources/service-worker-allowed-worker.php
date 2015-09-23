<?php
header('Content-Type: application/javascript');
if (isset($_GET['ServiceWorkerAllowed'])) {
  header('Service-Worker-Allowed: ' . $_GET['ServiceWorkerAllowed']);
}
?>
