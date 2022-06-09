# [Web Apps](../../README.md) - Manifest representations in code

This is a list of all the places where we represent
[manifest](https://w3c.github.io/manifest/) data in our codebase.

 - [blink.mojom.Manifest](../../../../third_party/blink/public/mojom/manifest/manifest.mojom)\
   Mojo IPC representation between Blink and the browser.
   Output of the [Blink manifest parser](../../../../third_party/blink/renderer/modules/manifest/manifest_parser.cc).

 - [blink::Manifest](../../../../third_party/blink/public/common/manifest/manifest.h)\
   Pre blink.mojom.Manifest representation that is getting cleaned up: https://crbug.com/1233362

 - [WebApplicationInfo](../web_application_info.h)\
   Used for installation and updates.

 - [web_app::WebApp](../web_app.h)\
   Installed web app representation in RAM.

 - [web_app.WebAppProto](../proto/web_app.proto)\
   Installed web app representation on disk.

 - [sync_pb.WebAppSpecificsProto](../../../../components/sync/protocol/web_app_specifics.proto)\
   Installed web app representation in sync cloud.

 - [webapps.mojom.WebPageMetadata](../../../../components/webapps/common/web_page_metadata.mojom)\
   Manifest data provided by an HTML document.

 - [web_app::ParseOfflineManifest()](../preinstalled_web_app_utils.cc)\
   Custom JSON + PNG format for bundling WebApplicationInfo data on disk for offline default web app installation.

 - [WebApkInfo](../../android/webapk/webapk_info.h)\
   Web app installation data that was packaged in an APK.

 - [payments::WebAppInstallationInfo](../../../../components/payments/content/web_app_manifest.h)\
   Payments code doesn't live under /chrome/browser, they have their own parser and representation.
