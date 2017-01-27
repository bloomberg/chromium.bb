<?php
if (isset($_GET['RedirectError'])) {
  header('Location: dummy.html');
  header('HTTP/1.0 302 Found');
} if (isset($_GET['BrokenChunked'])) {
  header("Content-type: text/html; charset=UTF-8");
  header("Transfer-encoding: chunked");
  echo "hello\nworld\n";
} else {
  header("Content-Type: text/html; charset=UTF-8");
  echo "OK";
}
?>
