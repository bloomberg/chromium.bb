<?php
header('HTTP/1.1 302 Found');
header('Location: ' . ($_SERVER['HTTP_ORIGIN'] ? 'square100.png' : 'square200.png'));
?>
