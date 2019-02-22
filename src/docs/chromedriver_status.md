# WebDriver Status

Below is a list of all WebDriver commands and their current support in ChromeDriver based on what is in the [WebDriver Specification](https://w3c.github.io/webdriver/webdriver-spec.html).

| Method | URL | Command | Status | Bug
| --- | --- | --- | --- | --- |
| POST   | /session                                                       | New Session                | Partially Complete | [1997](https://bugs.chromium.org/p/chromedriver/issues/detail?id=1997) [2537](https://bugs.chromium.org/p/chromedriver/issues/detail?id=2537)
| DELETE | /session/{session id}                                          | Delete Status              | Complete           |
| GET    | /status                                                        | Status                     | Complete           |
| GET    | /session/{session id}/timeouts                                 | Get Timeouts               | Complete           |
| POST   | /session/{session id}/timeouts                                 | Set Timeouts               | Complete           |
| POST   | /session/{session id}/url                                      | Navigate To                | Complete           | 
| GET    | /session/{session id}/url                                      | Get Current URL            | Complete           |
| POST   | /session/{session id}/back                                     | Back                       |                    |
| POST   | /session/{session id}/forward                                  | Forward                    |                    |
| POST   | /session/{session id}/refresh                                  | Refresh                    | Partially Complete | [1988](https://bugs.chromium.org/p/chromedriver/issues/detail?id=1988)
| GET    | /session/{session id}/title                                    | Get Title                  | Complete           |
| GET    | /session/{session id}/window                                   | Get Window Handle          | Complete           |
| DELETE | /session/{session id}/window                                   | Close Window               | Complete           |
| POST   | /session/{session id}/window                                   | Switch To Window           | Complete           |
| GET    | /session/{session id}/window/handles                           | Get Window Handles         | Complete           |
| POST   | /session/{session id}/frame                                    | Switch To Frame            | Complete           | 
| POST   | /session/{session id}/frame/parent                             | Switch To Parent Frame     |                    |
| GET    | /session/{session id}/window/rect                              | Get Window Rect            | Complete           |
| POST   | /session/{session id}/window/rect                              | Set Window Rect            | Complete           |
| POST   | /session/{session id}/window/maximize                          | Maximize Window            | Complete           |
| POST   | /session/{session id}/window/minimize                          | Minimize Window            | Complete           |
| POST   | /session/{session id}/window/fullscreen                        | Fullscreen Window          | Partially Complete | [1993](https://bugs.chromium.org/p/chromedriver/issues/detail?id=1993)
| GET    | /session/{session id}/element/active                           | Get Active Element         | Complete           |
| POST   | /session/{session id}/element                                  | Find Element               |                    |
| POST   | /session/{session id}/elements                                 | Find Elements              |                    |
| POST   | /session/{session id}/element/{element id}/element             | Find Element From Element  |                    |
| POST   | /session/{session id}/element/{element id}/elements            | Find Elements From Element |                    |
| GET    | /session/{session id}/element/{element id}/selected            | Is Element Selected        |                    |
| GET    | /session/{session id}/element/{element id}/attribute/{name}    | Get Element Attribute      |                    |
| GET    | /session/{session id}/element/{element id}/property/{name}     | Get Element Property       | Complete           | 
| GET    | /session/{session id}/element/{element id}/css/{property name} | Get Element CSS Value      | Partially Complete | [1994](https://bugs.chromium.org/p/chromedriver/issues/detail?id=1994)
| GET    | /session/{session id}/element/{element id}/text                | Get Element Text           | Complete           |
| GET    | /session/{session id}/element/{element id}/name	              | Get Element Tag Name       | Complete           |
| GET    | /session/{session id}/element/{element id}/rect                | Get Element Rect           | Complete           |
| GET    | /session/{session id}/element/{element id}/enabled             | Is Element Enabled         | Partially Complete | [1995](https://bugs.chromium.org/p/chromedriver/issues/detail?id=1995)
| POST   | /session/{session id}/element/{element id}/click               | Element Click              | Partially Complete | [1996](https://bugs.chromium.org/p/chromedriver/issues/detail?id=1996)
| POST   | /session/{session id}/element/{element id}/clear               | Element Clear              | Partially Complete | [1998](https://bugs.chromium.org/p/chromedriver/issues/detail?id=1998)
| POST   | /session/{session id}/element/{element id}/value               | Element Send Keys          | Partially Complete | [1999](https://bugs.chromium.org/p/chromedriver/issues/detail?id=1999)
| GET    | /session/{session id}/source                                   | Get Page Source            |                    |
| POST   | /session/{session id}/execute/sync                             | Execute Script             | Partially Complete | [2398](https://bugs.chromium.org/p/chromedriver/issues/detail?id=2398) [2556](https://bugs.chromium.org/p/chromedriver/issues/detail?id=2556)
| POST   | /session/{session id}/execute/async                            | Execute Async Script       | Partially Complete | [2398](https://bugs.chromium.org/p/chromedriver/issues/detail?id=2398) [2556](https://bugs.chromium.org/p/chromedriver/issues/detail?id=2556)
| GET    | /session/{session id}/cookie                                   | Get All Cookies            | Complete           |
| GET    | /session/{session id}/cookie/{name}                            | Get Named Cookie           | Complete           |
| POST   | /session/{session id}/cookie                                   | Add Cookie                 | Partially Complete | [2002](https://bugs.chromium.org/p/chromedriver/issues/detail?id=2002)
| DELETE | /session/{session id}/cookie/{name}                            | Delete Cookie              | Complete           |
| DELETE | /session/{session id)/cookie                                   | Delete All Cookies         | Complete           |
| POST   | /session/{session id}/actions                                  | Perform Actions            | Incomplete         | [1897](https://bugs.chromium.org/p/chromedriver/issues/detail?id=1897)
| DELETE | /session/{session id}/actions                                  | Release Actions            | Incomplete         | [1897](https://bugs.chromium.org/p/chromedriver/issues/detail?id=1897)
| POST   | /session/{session id}/alert/dismiss                            | Dismiss Alert              | Complete           |
| POST   | /session/{session id}/alert/accept                             | Accept Alert               | Complete           |
| GET    | /session/{session id}/alert/text                               | Get Alert Text             | Complete           |
| POST   | /session/{session id}/alert/text                               | Send Alert Text            | Complete           |
| GET    | /session/{session id}/screenshot                               | Take Screenshot            |                    |
| GET    | /session/{session id}/element/{element id}/screenshot          | Take Element Screenshot    | Complete           |
