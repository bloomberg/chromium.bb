<?php
if (isset($_SERVER['HTTP_RANGE']) && ($_SERVER['HTTP_RANGE'] == 'bytes=0-')) {
  header('HTTP/1.0 206 Partial Content');
  header('Content-Type: audio/ogg');
  # 12983 is the file size of media/content/silence.oga.
  header('Content-Range: bytes 0-2/12983');
  echo 'Ogg';
  return;
}
header('HTTP/1.1 307 Temporary Redirect');
header('Location: ' . $_GET["location"]);
?>
