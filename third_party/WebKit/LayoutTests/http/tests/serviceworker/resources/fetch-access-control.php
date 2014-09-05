<?php
header('X-ServiceWorker-ServerHeader: SetInTheServer');
if (isset($_GET['ACAOrigin'])) {
    $origins = explode(',', $_GET['ACAOrigin']);
    for ($i = 0; $i < sizeof($origins); ++$i)
        header("Access-Control-Allow-Origin: " . $origins[$i], false);
}

if (isset($_GET['ACAHeaders']))
    header("Access-Control-Allow-Headers: {$_GET['ACAHeaders']}");
if (isset($_GET['ACAMethods']))
    header("Access-Control-Allow-Methods: {$_GET['ACAMethods']}");
if (isset($_GET['ACACredentials']))
    header("Access-Control-Allow-Credentials: {$_GET['ACACredentials']}");
if (isset($_GET['ACEHeaders']))
    header("Access-Control-Expose-Headers: {$_GET['ACEHeaders']}");

if ((isset($_GET['Auth']) and !isset($_SERVER['PHP_AUTH_USER'])) || isset($_GET['AuthFail'])) {
    header('WWW-Authenticate: Basic realm="Restricted"');
    header('HTTP/1.0 401 Unauthorized');
    echo 'Authentication canceled';
    exit;
}

if (isset($_GET['PNGIMAGE'])) {
  header('Content-Type: image/png');
  echo base64_decode(
    'iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAAXNSR0IArs4c6QAAAARnQU1B' .
    'AACxjwv8YQUAAAAJcEhZcwAADsQAAA7EAZUrDhsAAAAhSURBVDhPY3wro/KfgQLABKXJBqMG' .
    'jBoAAqMGDLwBDAwAEsoCTFWunmQAAAAASUVORK5CYII=');
  exit;
}

$username = 'undefined';
$password = 'undefined';
$cookie = 'undefined';
if (isset($_SERVER['PHP_AUTH_USER'])) {
    $username = $_SERVER['PHP_AUTH_USER'];
}
if (isset($_SERVER['PHP_AUTH_PW'])) {
    $password = $_SERVER['PHP_AUTH_PW'];
}
if (isset($_COOKIE['cookie'])) {
    $cookie = $_COOKIE['cookie'];
}

$files = array();
foreach ($_FILES as $key => $file) {
    $content = '';
    $fp = fopen($file['tmp_name'], 'r');
    if ($fp) {
        $content = $file['size'] > 0 ? fread($fp, $file['size']) : '';
        fclose($fp);
    }
    $files[] = array('key' => $key,
                     'name' => $file['name'],
                     'type' => $file['type'],
                     'error' => $file['error'],
                     'size' => $file['size'],
                     'content' => $content);
}

header('Content-Type: application/json');
$arr = array('jsonpResult' => 'success',
             'method' => $_SERVER['REQUEST_METHOD'],
             'headers' => getallheaders(),
             'body' => file_get_contents('php://input'),
             'files' => $files,
             'get' => $_GET,
             'post' => $_POST,
             'username' => $username,
             'password' => $password,
             'cookie' => $cookie);
$json = json_encode($arr);
echo "report( $json );";
?>