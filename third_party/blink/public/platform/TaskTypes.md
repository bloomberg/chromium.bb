# Task Queues

Blink uses a series of task queues.

All specified (in W3C, HTML, DOM, etc) task queues are pausable. Some internal task queues are not.

| Task Queue           | Pausable | Throttlable |     Frozen   | Deferred |
| -------------------- | -------- | ----------- | ------------ | -------- |
| DOM Manipulation     | Yes      | Yes         | Android Only |  Yes     |
| User Interaction     | Yes      | Yes         | Android Only |  No      |
| Networking           | Yes      | No          | Yes          |  Yes     |
| Networking (URL Ldr) | Yes      | No          | Yes          |  Yes     |
| Networking (Ctrl)    | Yes      | No          | Yes          |  Yes     |
| History Traversal    | Yes      | Yes         | Android Only |  Yes     |
| Embed                | Yes      | Yes         | Android Only |  Yes     |
| Media Element        | Yes      | Yes         | Android Only |  No      |
| Canvas Block Serial. | Yes      | Yes         | Android Only |  Yes     |
| Microtask            | Yes      | Yes         | Android Only |  Yes     |
| Javascript Timer     | Yes      | Yes         | Yes          |  Yes     |
| Remote Event         | Yes      | Yes         | Android Only |  Yes     |
| Web Socket           | Yes      | Yes         | Android Only |  Yes     |
| Posted Message       | Yes      | Yes         | Android Only |  No      |
| Unshipped Port Msg.  | Yes      | Yes         | Android Only |  Yes     |
| File Reading         | Yes      | Yes         | Android Only |  Yes     |
| Database Access      | Yes      | Yes         | Android Only |  No      |
| Presentation         | Yes      | Yes         | Android Only |  Yes     |
| Sensor               | Yes      | Yes         | Android Only |  Yes     |
| Performance Timeline | Yes      | Yes         | Android Only |  Yes     |
| WebGL                | Yes      | Yes         | Android Only |  Yes     |
| Idle Task            | Yes      | Yes         | Android Only |  Yes     |
| Misc Platform API    | Yes      | Yes         | Android Only |  Yes     |
| Worker Animation     | Yes      | Yes         | Android Only |  No      |
| Web Schdlr User Int. | Yes      | Yes         | Yes          |  No      |
| Web Schdlr Best Eff. | Yes      | Yes         | Yes          |  Yes     |
| Font Loading         | Yes      | Yes         | Android Only |  Yes     |
| Application Lifecycle| Yes      | Yes         | Android Only |  Yes     |
| Background Fetch     | Yes      | Yes         | Android Only |  Yes     |
| Permission           | Yes      | Yes         | Android Only |  Yes     |
| Service Worklet CMsg | Yes      | Yes         | Android Only |  No      |
| Internal Default     | No       | No          | No           |  No      |
| Internal Loading     | No       | No          | No           |  No      |
| Internal Test        | No       | No          | No           |  No      |
| Internal Web Crypto  | Yes      | Yes         | Android Only |  No      |
| Internal Media       | Yes      | Yes         | Android Only |  No      |
| Internal Media Rt.   | Yes      | Yes         | Android Only |  No      |
| Internal IPC         | No       | No          | No           |  No      |
| Internal User Inter. | Yes      | Yes         | Android Only |  No      |
| Internal Inspector   | No       | No          | No           |  No      |
| Internal Worker      | No       | No          | No           |  No      |
| Internal Translation | No       | No          | No           |  No      |
| Internal Intersec Obs| Yes      | Yes         | Android Only |  No      |
| Internal Content Cpt | Yes      | Yes         | Yes          | Yes      |
| Internal Nav         | No       | No          | No           |  No      |
| Main Thread V8       | No       | No          | No           |  No      |
| Main Thread Composit.| No       | No          | No           |  No      |
| Main Thread Default  | No       | No          | No           |  No      |
| Main Thread Input    | No       | No          | No           |  No      |
| Main Thread Idle     | No       | No          | No           |  No      |
| Main Thread Control  | No       | No          | No           |  No      |
| Main Thread Cleanup  | No       | No          | No           |  No      |
| Main Thread Mem Purge| No       | No          | No           |  No      |
| Compositor Default   | No       | No          | No           |  No      |
| Compositor Input     | No       | No          | No           |  No      |
| Worker Default       | No       | No          | No           |  No      |
| Worker V8            | No       | No          | No           |  No      |
| Worker Compositor    | No       | No          | No           |  No      |

Internal Translation queue supports concept of it running only in the foreground. It is disabled if the page that owns it goes in background.
