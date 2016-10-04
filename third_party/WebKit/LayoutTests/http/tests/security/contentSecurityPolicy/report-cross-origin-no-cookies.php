<?php
header("Content-Security-Policy: img-src 'none'; report-uri http://localhost:8080/security/contentSecurityPolicy/resources/save-report.php?test=report-cross-origin-no-cookies.php");
?>
<!DOCTYPE html>
<html>
<head>
  <script src="resources/report-test.js"></script>
</head>
<body>
<script>
if (window.testRunner) {
  testRunner.dumpAsText();
  testRunner.waitUntilDone();
  testRunner.setBlockThirdPartyCookies(false);
}

fetch(
    "http://localhost:8080/security/resources/set-cookie.php?name=cspViolationReportCookie&value=crossOrigin",
    {mode: 'no-cors', credentials: 'include'})
  .then(() => {
    // This image will generate a CSP violation report.
    const img = new Image();

    img.onerror = () => {
      window.location = "/security/contentSecurityPolicy/resources/echo-report.php?test=report-cross-origin-no-cookies.php";
    };
    img.src = "/security/resources/abe.png";
    document.body.appendChild(img);
  });
</script>
</body>
</html>
