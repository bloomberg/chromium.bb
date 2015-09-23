<?php
  $step = $_GET["step"] ? $_GET["step"] : 1;
  $pageOrigin = $_GET["pageOrigin"];
  $crossOrigin = $_GET["crossOrigin"];
  $timingAllow = $_GET["timingAllow"] ? $_GET["timingAllow"] : 0;
  $redirectURL = "/w3c/webperf/resources/multi_redirect.php?pageOrigin=$pageOrigin&crossOrigin=$crossOrigin&timingAllow=$timingAllow";

  switch ($step) {
    case 1:
      $redirectURL = $crossOrigin . $redirectURL . "&step=2";
      if ($timingAllow != "0")
        header("timing-allow-origin: $pageOrigin");
      break;
    case 2:
      $redirectURL = $pageOrigin . $redirectURL . "&step=3";
      if ($timingAllow != "0")
        header("timing-allow-origin: $pageOrigin");
      break;
    case 3:
      $redirectURL = $pageOrigin . "/w3c/webperf/resources/blank_page_green.htm";

      break;
    default:
      break;
  }

  header("HTTP/1.1 302");
  header("Location: $redirectURL");
  sleep(1);
  exit;
?>
