<?php
header('Content-Type: image/png');
if ($_SERVER['HTTP_REFERER'] == $_GET['referrer']) {
    $img = 'green250x50.png';
} else {
    $img = 'red200x100.png';
}
echo file_get_contents($img);
?>
