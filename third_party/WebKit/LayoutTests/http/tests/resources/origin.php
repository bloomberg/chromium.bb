<?php

$gotOrigin = 0;
foreach (getallheaders() as $name => $value) {
    if ($name == "Origin") {
        $gotOrigin = 1;
    }
}

if ($gotOrigin) {
    header('Location: square100.png');
}
else {
    header('Location: square200.png');
}
header('HTTP/1.1 302 Redirect');
