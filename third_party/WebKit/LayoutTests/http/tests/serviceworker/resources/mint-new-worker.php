<?php
// Mint a new worker each load.
header("Cache-Control: no-cache, must-revalidate");
header("Pragma: no-cache");
header('Content-Type:application/javascript');
echo "// " . microtime() . "\n";

if ($_SERVER['QUERY_STRING'] == 'skip-waiting') {
  echo "skipWaiting();\n";
}
?>
onactivate = (e) => e.waitUntil(clients.claim());

var resolve_wait_until;
var wait_until = new Promise(resolve => {
    resolve_wait_until = resolve;
  });
onmessage = (e) => {
    if (e.data == 'wait')
      e.waitUntil(wait_until);
    if (e.data == 'go')
      resolve_wait_until();
  };
