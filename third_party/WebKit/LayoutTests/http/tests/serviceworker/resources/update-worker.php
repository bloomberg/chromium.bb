<?php
if(!isset($_COOKIE['mode']))
  $mode = 'init'; // Set mode to 'init' for initial fetch.
else
  $mode = $_COOKIE['mode']; // $_COOKIE['mode'] is either 'normal' or 'error'.

// no-cache itself to ensure the user agent finds a new version for each update.
header("Cache-Control: no-cache, must-revalidate");
header("Pragma: no-cache");

if ($mode == 'init') {
  // Set a normal mimetype.
  // Set cookie value to 'normal' so the next fetch will work in 'normal' mode.
  header('Content-Type:application/javascript');
  setcookie('mode', 'normal');
} else if ($mode == 'normal') {
  // Set a normal mimetype.
  // Set cookie value to 'error' so the next fetch will work in 'error' mode.
  header('Content-Type:application/javascript');
  setcookie('mode', 'error');
} else if ($mode == 'error') {
  // Set a disallowed mimetype.
  // Unset and delete cookie to clean up the test setting.
  header('Content-Type:text/html');
  unset($_COOKIE['mode']);
  setcookie('mode', '', time() - 3600);
}
// Return a different script for each access.
echo '// ' . microtime();
?>
