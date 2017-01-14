<?php
if (isset($_GET["origintrial"])) {
  // generate_token.py  http://127.0.0.1:8000 ServiceWorkerNavigationPreload -expire-timestamp=2000000000
  header("Origin-Trial: AsAA4dg2Rm+GSgnpyxxnpVk1Bk8CcE+qVBTDpPbIFNscyNRJOdqw1l0vkC4dtsGm1tmP4ZDAKwycQDzsc9xr7gMAAABmeyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiU2VydmljZVdvcmtlck5hdmlnYXRpb25QcmVsb2FkIiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9");
}
header('Content-Type: application/javascript');
?>
importScripts('./get-interface-names.js');

self.addEventListener('message', event => {
    event.source.postMessage(
        get_interface_names(
            this,
            ['NavigationPreloadManager',
             'FetchEvent',
             'ServiceWorkerRegistration']));
  });
