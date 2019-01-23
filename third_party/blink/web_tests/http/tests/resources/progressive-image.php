<?php
$name = $_GET['name'];
$sleepTime = $_GET['sleep'];
$numInitial = $_GET['numInitial'];

header('Content-Type: image');
header('Content-Length: ' . filesize($name));
// Read from the beginning, |numInitial| bytes.
$first = file_get_contents($name, FALSE, NULL, 0, $numInitial);
echo $first;
flush();

usleep($sleepTime*1000);

// Read the remainder after having slept.
$second = file_get_contents($name, FALSE, NULL, $numInitial);
echo $second;
