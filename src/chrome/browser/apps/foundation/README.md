# Chrome Apps Platform Foundation

Chrome features multiple app platforms, including PWAs (desktop and mobile),
ARC++ (Chrome OS), and Chrome apps (now deprecated). Each of these is based on
different underlying technologies, but UX-wise, it is an explicit goal to
minimize the differences between them and give users the feeling that all apps
have as similar properties, management, and features as possible.

This directory contains generic code to facilitate app platforms on Chrome
communicating with the browser and each other. This layer consists of
app-platform-agnostic code, which each app platform's custom implementation
then plugs into. Example features include:

* `app_service` - a central service for information on installed apps
* `app_registry` - the central registry for installed apps contained within
the app service

Over time, it is intended to reduce the dependencies of this layer on Chrome
browser through refactoring work, with the eventual goal of moving this
directory to chrome/services.
