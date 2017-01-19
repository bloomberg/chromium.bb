<?php
  function put_chunk($txt) {
    echo sprintf("%x\r\n", strlen($txt));
    echo "$txt\r\n";
  }
  header("Content-type: text/html; charset=UTF-8");
  header("Transfer-encoding: chunked");
  for ($i = 0; $i < 10; $i++) {
    // Call ob_flush(), flush() and usleep() to send the response as multiple
    // chunks.
    ob_flush();
    flush();
    usleep(1000);
    put_chunk("$i");
  }
  echo "0\r\n\r\n";
?>
