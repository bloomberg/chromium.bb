<?php 
header('Content-Type:application/javascript');
header("Transfer-encoding: chunked");
flush();
sleep(1);
echo "XX\r\n\r\n";
?>
