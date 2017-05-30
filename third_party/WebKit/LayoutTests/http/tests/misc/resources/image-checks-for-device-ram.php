<?php
    $device_ram = $_SERVER["HTTP_DEVICE_RAM"];

    if(isset($device_ram) && $device_ram != 0
    	 && ($device_ram & ($device_ram - 1)) == 0) {
        $fn = fopen("compass.jpg", "r");
        fpassthru($fn);
        fclose($fn);
        exit;
    }
    header("HTTP/1.1 417 Expectation failed");
?>
