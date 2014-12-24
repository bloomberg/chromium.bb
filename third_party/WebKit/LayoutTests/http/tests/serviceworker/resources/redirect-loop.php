<?php
$url = $_GET['Redirect'];
$path = '/serviceworker/resources/redirect-loop.php';
if (isset($_GET['Count'])) {
  $count = intval($_GET['Count']) - 1;
  if ($count > 0) {
    $url = $path .
           '?Redirect=' . rawurlencode($url) .
           '&Count=' . $count ;
  }
}
header("Location: $url");
?>
