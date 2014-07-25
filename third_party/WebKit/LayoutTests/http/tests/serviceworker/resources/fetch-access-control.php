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

header('Content-Type: application/json');
$arr = array('jsonpResult' => 'success',
             'method' => $_SERVER['REQUEST_METHOD'],
             'headers' => getallheaders(),
             'username' => $username,
             'password' => $password,
             'cookie' => $cookie);
$json = json_encode($arr);
echo "report( $json );";
?>