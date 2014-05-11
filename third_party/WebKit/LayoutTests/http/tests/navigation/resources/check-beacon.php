<?php
$beaconFilename = "beacon" . (isset($_REQUEST['name']) ? $_REQUEST['name'] : "") . ".txt";
$retries = isset($_REQUEST['retries']) ? (int)$_REQUEST['retries'] : -1;
while (!file_exists($beaconFilename) && $retries != 0) {
    usleep(10000);
    # file_exists() caches results, we want to invalidate the cache.
    clearstatcache();
    $retries--;
}

header('Content-Type: text/plain');
header('Access-Control-Allow-Origin: *');
if (file_exists($beaconFilename)) {
    echo "Beacon sent successfully\n";
    $beaconFile = fopen($beaconFilename, 'r');
    while ($line = fgets($beaconFile)) {
        $trimmed = trim($line);
        if ($trimmed != "")
            echo "$trimmed\n";
    }
    fclose($beaconFile);
    unlink($beaconFilename);
} else {
    echo "Beacon not sent\n";
}
?>
