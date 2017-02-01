<?php
if (isset($_GET["origintrial"])) {
  // Generate token with the command:
  // generate_token.py  http://127.0.0.1:8000 ForeignFetch -expire-timestamp=2000000000
  header("Origin-Trial: AinuK1QNcHGJANXfN0Pzkvpxm8rocd4kcqQqhp7rBN8PEX/1JicCoDXX3fIhk7iwBOCCOQYR0cvZ8XHTLJJfVw0AAABUeyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiRm9yZWlnbkZldGNoIiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9");
}
header('Content-Type: application/javascript');
?>
importScripts('../../../resources/origin-trials-helper.js');

self.addEventListener('message', event => {
    event.source.postMessage(
        OriginTrialsHelper.get_interface_names(
            this,
            ['ForeignFetchEvent',
             'InstallEvent',
             'global'
            ],
            {'InstallEvent':['registerForeignFetch'],
             'global':['onforeignfetch']}));
  });
