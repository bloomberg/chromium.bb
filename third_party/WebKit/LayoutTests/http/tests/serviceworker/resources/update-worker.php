<?php
if(!isset($_COOKIE['mode']))
  $mode = 'init'; // Set mode to 'init' for initial fetch.
else
  $mode = $_COOKIE['mode']; // $_COOKIE['mode'] is either 'normal' or 'error'.

// no-cache itself to ensure the user agent finds a new version for each update.
header("Cache-Control: no-cache, must-revalidate");
header("Pragma: no-cache");

$extra_body = '';

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
  // Set cookie value to 'syntax-error' so the next fetch will work in 'syntax-error' mode.
  header('Content-Type:text/html');
  setcookie('mode', 'syntax-error');
} else if ($mode == 'syntax-error') {
  // Set cookie value to 'throw-install' so the next fetch will work in 'throw-install' mode.
  header('Content-Type:application/javascript');
  setcookie('mode', 'throw-install');
  $extra_body = 'badsyntax(isbad;';
} else if ($mode == 'throw-install') {
  // Unset and delete cookie to clean up the test setting.
  header('Content-Type:application/javascript');
  unset($_COOKIE['mode']);
  setcookie('mode', '', time() - 3600);
  $extra_body = "addEventListener('install', function(e) { throw new Error('boom'); });";
}
// Return a different script for each access.
echo '/* ', microtime(), ' */ ', $extra_body;
?>
