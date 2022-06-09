---
breadcrumbs:
- - /Home
  - Chromium
- - /Home/chromium-security
  - Chromium Security
page_name: quarterly-updates
title: Quarterly Updates
---

We post a newsletter-y update quarterly on
[security-dev@chromium.org](https://groups.google.com/a/chromium.org/forum/#!searchin/security-dev/quarter$20summary%7Csort:date).
It's an open list, so
[subscribe](https://groups.google.com/a/chromium.org/forum/#!forum/security-dev)
if you're interested in updates, discussion, or feisty rants related to Chromium
security.

## Q1 and Q2 2021

Greetings,

With apologies to those still waiting patiently for our Q1 update, here instead
is a look back at what the Chrome Security teams have been up to in the first
half of 2021.

Chrome is hiring, including for security positions! See
[g.co/chrome/hiring](https://g.co/chrome/hiring). In particular we're looking
for a [lead security product
manager](https://careers.google.com/jobs/results/118648881425588934-lead-product-manager-chrome-security/)
to work with the teams doing all the great things in this update, and more
across the Chrome Trust and Safety organisation.

The first half of 2021 is trending toward record-setting totals for the Chrome
[Vulnerability Reward Program](https://g.co/chrome/vrp) (VRP) with the security
researcher community awarded $1.7M for reporting close to 200 unique, valid
security bugs. Of these reward-eligible reports, 84 were reports for Critical
and High severity issues that impacted stable channel users. The Chrome VRP
continues to be a vital part of our security ecosystem and we greatly appreciate
the efforts of the Chrome VRP researcher community to help keep Chrome users
more secure!

In a collaborative effort led by the Google VRP, the new [Google BugHunters
site](https://bughunters.google.com/) was launched. Chrome bugs can be reported
via that site, as well as at [crbug.com/new](https://crbug.com/new) using the
Security Bug template as before.

In collaboration with the other VRP programs across Google, bonuses were paid
out to VRP researchers impacted by recent payment delays. We are additionally
working on ways to proactively decrease future delays, and improve the
efficiency and processes of the program.

In Q1 the Safe Browsing team grew the [Enhanced Safe
Browsing](https://security.googleblog.com/2020/05/enhanced-safe-browsing-protection-now.html)
population by more than 400% through in-product integrations with the security
interstitial pages and Safety Check. We also started using machine learning
models to protect users who have [real-time Safe Browsing
lookups](https://security.googleblog.com/2019/12/better-password-protections-in-chrome.html#:~:text=real-time%20phishing%20protection:%20checking%20with%20safe%20browsing%E2%80%99s%20blocklist%20in%20real%20time.)
against phishing attacks which, along with heuristic-based enforcement, allowed
us to decrease our phishing false negatives by up to 20%.

We designed improvements to our client-side phishing detection subsystem, which
will allow us to innovate faster in that area in the coming quarters.

In Q2 we [rolled
out](https://security.googleblog.com/2021/06/new-protections-for-enhanced-safe.html)
a new set of protections for Enhanced Safe Browsing users in Chrome 91: Improved
download protection by offering scanning of suspicious downloads, and better
protection against untrusted extensions. We continued to see a phenomenal growth
in the number of users who opt in to Enhanced Safe Browsing to get Chrome’s
highest level of security.

We helped land improvements to the client-side phishing detection subsystem in
Chrome 92 which made image-based phishing classification [up to 50 times
faster](https://blog.chromium.org/2021/07/m92-faster-and-more-efficient-phishing-detection.html).
And we landed improvements to the Chrome Cleanup Tool to remove new families of
unwanted software from the users’ machines.

In Chrome 90, we launched a
[milestone](https://blog.chromium.org/2021/03/a-safer-default-for-navigation-https.html)
for a secure web: Chrome’s omnibox now defaults to HTTPS when users don’t
specify a scheme. We later
[announced](https://blog.chromium.org/2021/07/increasing-https-adoption.html) a
set of changes to prepare the web for an HTTPS-first future. We’re implementing
HTTPS-First Mode as an option for Chrome 94, a setting that will cause Chrome to
automatically upgrade navigations to HTTPS, and show a full-page warning before
falling back to HTTP. We’ll also be experimenting with a new security indicator
icon for HTTPS pages in Chrome 93, inspired by our research showing that many
users don’t understand the security assurances of the padlock icon. Finally, we
announced a set of guiding principles for protecting and informing users on the
slice of the web that is still HTTP.

To try out HTTPS-First Mode in Chrome Canary, toggle “Always use secure
connections” in chrome://settings/security. You can also preview our new HTTPS
security indicator by enabling “Omnibox Updated connection security indicators”
in chrome://flags and then re-launching Chrome.

In June, Chrome passed a huge milestone in the history of [Certificate
Transparency](https://certificate.transparency.dev/). The last certificates
issued before Chrome required CT logging have now expired. That eliminates a
hole where a malicious or compromised CA key could backdate a cert to avoid
logging it. Congratulations to all who've worked on CT over the years, and those
who continue to keep the ecosystem thriving.

To further strengthen the Certificate Transparency ecosystem, we launched the
[first
phase](https://docs.google.com/document/d/1G1Jy8LJgSqJ-B673GnTYIG4b7XRw2ZLtvvSlrqFcl4A/edit#heading=h.7nki9mck5t64)
of SCT auditing, which helps verify that CT logs are behaving honestly, and
[designed](https://docs.google.com/document/d/1YTUzoG6BDF1QIxosaQDp2H5IzYY7_fwH8qNJXSVX8OQ/edit)
and began implementing subsequent phases to improve coverage and reliability. In
Chrome 94 we’ll launch a change to distribute CT log information to clients
faster and more reliably, which will help unblock CT enforcement on Chrome for
Android.

We’ve made progress on under-the-hood improvements to certificates and TLS. We
[proposed](https://github.com/sleevi/cabforum-docs/tree/profiles) a set of
changes in the [CA/Browser Forum](https://cabforum.org/) to better specify how
website (and other) certificates should be structured, and we
[helped](https://cabforum.org/2021/04/22/ballot-sc42-398-day-re-use-period/)
[make](https://cabforum.org/2021/06/02/ballot-sc46-sunset-the-caa-exception-for-dns-operator/)
[improvements](https://cabforum.org/2021/05/01/ballot-sc44-clarify-acceptable-status-codes/)
such as
[tightening](https://cabforum.org/2021/06/03/ballot-sc45-wildcard-domain-validation/)
validation procedures for wildcard certificates and
[sunsetting](https://cabforum.org/2021/06/30/ballot-sc47v2-sunset-subjectorganizationalunitname/)
an unvalidated certificate field. We distrusted the Camerfirma CA, initially
planned for Chrome 90 but later delayed until Chrome 91 due to the exceptional
circumstances of some Covid-19 related government websites being slow to
migrate.

On the TLS front, we launched a performance improvement to the latest version of
TLS — [zero round-trip
handshakes](https://www.chromestatus.com/feature/5447945241493504) in TLS 1.3 —
to Canary and Dev. We
[announced](https://groups.google.com/a/chromium.org/g/blink-dev/c/RShdgyaDoX4/m/JikQYHPuBQAJ)
and implemented the removal of the obsolete 3DES cipher. Finally, a new privacy
feature for TLS, [Encrypted Client
Hello](https://www.chromestatus.com/feature/6196703843581952), is now
implemented in our TLS library, with integration into Chrome ongoing.

The Open Web Platform Security team implemented and specced a first version of
the [Sanitizer API](https://wicg.github.io/sanitizer-api/), that will help
developers avoid pesky XSS bugs. In combination with [Trusted
Types](https://web.dev/trusted-types/#:~:text=Trusted%20Types%20give%20you%20the,is%20available%20for%20other%20browsers.)
that we released last year, it will help websites defend against XSS attacks.

CORS-RFC1918 got renamed to Private Network Access. We are ready to ship
[restrictions on accessing resources from private networks from public HTTP
pages](https://developer.chrome.com/blog/private-network-access-update/) in
Chrome 94: public HTTP pages will no longer be able to request resources from
private networks. We will have a reverse Origin Trial in place until our
preferred workaround (WebTransport) has shipped. We are also working on the next
stage of Private Network Access restrictions, where we will send a CORS
preflight when a public page tries to access a private resource.

CrossOriginIsolated is really difficult to adopt for websites. We’re planning to
make a few changes to help with deployment. First, we have a new version of
[COEP](https://html.spec.whatwg.org/multipage/origin.html#coep):
[credentialless](https://developer.chrome.com/blog/coep-credentialless-origin-trial/)
currently undergoing an Origin Trial. It will help developers deploy COEP when
they embed third-party subresources. We’re also working on [anonymous
iframes](https://github.com/camillelamy/explainers/blob/main/anonymous_iframes.md),
to deploy COEP on pages that embed legacy 3rd party iframes. And we want to have
[COOP same-origin-allow-popups + COEP enable
crossOriginIsolated](https://github.com/camillelamy/explainers/blob/main/coi-with-popups.md)
to help with OAuth and payment flows support.

In Q1, the Security Architecture team continued work on several Site Isolation
efforts: isolating sites that use [COOP](https://crbug.com/1018656) or
[OAuth](https://crbug.com/960888) on Android, metrics for protecting data with
[ORB](https://github.com/annevk/orb), better handling of about:blank origins and
tracking of content scripts, and helping with the communications for the
[Spectre
proof-of-concept](https://security.googleblog.com/2021/03/a-spectre-proof-of-concept-for-spectre.html)
and
[recommendations](https://blog.chromium.org/2021/03/mitigating-side-channel-attacks.html).
Additionally, [Origin Agent Cluster](https://web.dev/origin-agent-cluster/)
shipped in Chrome 88, offering process isolation at an origin granularity (for
performance reasons rather than security). We explored [new
options](https://chromium-review.googlesource.com/c/chromium/src/+/2782585) for
memory safety and helped with the
[MiraclePtr](https://docs.google.com/document/d/1pnnOAIz_DMWDI4oIOFoMAqLnf_MZ2GsrJNb_dbQ3ZBg/edit?usp=sharing)
experiments. Finally, we made several stability improvements, continued
refactoring for [SiteInstanceGroup](https://crbug.com/1195535), and helped
unblock the
[MPArch](https://docs.google.com/document/d/1NginQ8k0w3znuwTiJ5qjYmBKgZDekvEPC22q0I4swxQ/edit)
work.

Q2 saw work improving Site Isolation protections for Origin headers (via request
initiator enforcements) and extension IPCs. We started several Site Isolation
related beta trials, including isolating [more](https://crbug.com/1018656)
[sites](https://crbug.com/960888) on Android and [isolating
extensions](https://crbug.com/1209417) from each other on desktop. We started an
early prototype of [isolating same-site sandboxed
iframes](https://crbug.com/510122) and analyzed metrics for protecting data with
[ORB](https://github.com/annevk/orb), as well. We also contributed to several
efforts to improve memory safety in Chrome, solved long-standing speculative
RenderFrameHost crashes, and improved support for [Origin Agent
Cluster](https://web.dev/origin-agent-cluster/).

The Platform Security team had a busy first half of the year. We have now
deployed Hardware-enforced stack protection for Windows (also known as
Control-flow Enforcement Technology,
[CET](https://software.intel.com/content/www/us/en/develop/articles/technical-look-control-flow-enforcement-technology.html))
to most Chrome processes, on supported hardware. CET protects against control
flow attacks attempting to subvert the return from a function, and we
[blogged](https://security.googleblog.com/2021/05/enabling-hardware-enforced-stack.html)
about this earlier in the year.

With the returns from functions now protected by CET, we are making good headway
in protecting the function calls themselves — indirect calls, or 'icalls' using
[CFG](https://blog.trailofbits.com/2016/12/27/lets-talk-about-cfi-microsoft-edition/)
(Control Flow Guard). We have full CFG support for all processes behind a
compile time flag 'win_enable_cfg_guards = true', so please try it, but in the
meantime we are working on ironing out performance issues so we can roll it out
to as many processes as possible.

The stack canary mitigation has been significantly strengthened on Linux and
Chrome OS. These platforms use the zygote for launching new processes, so the
secret stack canary value was the same in each process, which means the
mitigation is useless once an attacker has taken over a single process. The
stack canaries are now re-randomized in each process.

On macOS, we finished our complete rollout of [our V2 sandbox
architecture](https://chromium.googlesource.com/chromium/src/sandbox/+/HEAD/mac/seatbelt_sandbox_design.md)
with the launch of the new GPU process sandbox. That marks the end of a nearly
four-year project to eliminate the unsandboxed warm-up phase of our processes,
which reduces the amount of attack surface available to a process. In addition,
we [enabled macOS
11’s](https://chromium.googlesource.com/chromium/src/+/46e23c8166086ef63e7d383149d4c91f30b7415e)
new [RIDL](https://mdsattacks.com/) CPU mitigation for processes that handle
untrustworthy arbitrary compute jobs (e.g. renderers).

GWP-ASAN is being field trialed on Linux, Chrome OS, and Android. GWP-ASAN is a
sampling allocation tool designed to detect heap memory errors occurring in
production with negligible overhead, providing allocation/deallocation/crashing
stack traces for production crashes. It has already been launched on macOS and
Windows but hopefully launching on new platforms should help us find and fix
bugs in platform-specific code.

XFA (the form-filling part of PDF) is now using Blink's garbage collector
[Oilpan](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/third_party/blink/renderer/platform/heap/BlinkGCAPIReference.md),
protecting against use-after-frees in this code. The PDF code is also being
moved from its own process that uses the legacy Pepper interface (previously
used for Flash) into the same process as web content.

The work on the network service sandbox continues apace. Previously the sandbox
technology being used on Windows was the same one used for the renderer (the
[restricted
token](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/design/sandbox.md#the-token)
sandbox). However, this tighter sandbox caused issues with parts of the network
stack such as Windows Authentication and
[SSPI](https://docs.microsoft.com/en-us/windows/win32/rpc/security-support-provider-interface-sspi-)
providers, so we are moving to an LPAC (Less Privilege App Container) sandbox
which should play much nicer with enterprises.

Speaking of enterprises, we landed a new set of policies to control the use of
the JIT (Just-In-Time) compiler in [V8](https://v8.dev/) (our JavaScript
engine). These policies allow enterprises to set a [default
policy](https://chromeenterprise.google/policies/#DefaultJavaScriptJitSetting)
and also to
[enable](https://chromeenterprise.google/policies/#JavaScriptJitAllowedForSites)
or
[disable](https://chromeenterprise.google/policies/#JavaScriptJitBlockedForSites)
JIT for certain sites. The V8 JIT has often been a juicy target for exploit
writers, and by not having any dynamically generated code we can also enable OS
mitigations such as CET (see above) and ACG (Arbitrary Code Guard) in renderer
processes to help prevent bugs from being turned into exploits as easily.
Disabling the JIT does have some drawbacks on web compatibility and performance
— but our friends in Edge subsequently wrote a great
[blog](https://microsoftedge.github.io/edgevr/posts/Super-Duper-Secure-Mode/)
exploring this debate which we encourage you to read before deploying this
policy.

In Q1 the extended team working on permissions was excited to start rolling out
the fruits of several collaborations from last year, including the MVP of the
Chrome Permission Suggestion Service (CPSS) to suppress
very-unlikely-to-be-granted prompts, the automatic revocation of notification
permission on abusive sites, a complete revamp of chrome://settings/content
pages, and experiments for Permission Chip and one-time permission grants. CPSS
reduces unwanted interruptions (number of explicit decisions which are
dismissed, denied or granted) by 20 to 30%. Additionally, the less disruptive
'chip' permission UI is now live for all users for location permission requests
and we’re migrating other permissions to the new pattern.

We organized a virtual workshop on next-gen permissions, identifying the core
themes – modes, automation, and awareness – for future explorations, and we
conducted our very first qualitative UXR study to better understand users’
mental models and expectations with permissions

We'll be back to our quarterly update cadence with news from Q3 later in the
year.

Cheers,

Andrew

## Q4 2020

## Greetings,

## Even as 2021 is well underway, here's a look back at what Chrome Security was up to in the last quarter of 2020.

## Interested in helping to protect users of Chrome, Chromium, and the entire web? We're hiring! Take a look at [g.co/chrome/hiring](https://g.co/chrome/hiring), with several of the roles in Washington, DC: [g.co/chrome/securityprivacydc](https://g.co/chrome/securityprivacydc).

## The Usable Security team fully launched a new warning for [lookalike domain names](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/lookalikes/lookalike-domains.md): low-quality or suspicious domains that make it hard for people to understand which website they’re actually visiting. We continued to place some final nails in the coffin of [mixed content](https://blog.chromium.org/2019/10/no-more-mixed-messages-about-https.html) (insecure subresources on secure pages). Secure pages are no longer allowed to initiate any [insecure downloads](https://blog.chromium.org/2020/02/protecting-users-from-insecure.html) as of Chrome 88. We uncovered some issues with our new [warning on mixed form submissions](https://blog.chromium.org/2020/08/protecting-google-chrome-users-from.html) due to redirects, and this warning will be re-launching in Chrome 88 as well.

## With HTTPS adoption continuing to rise, it’s now time to begin treating https:// as the default protocol, so we began implementing a change to the Chrome address bar to default to https:// instead of http:// if the user doesn’t type a scheme. Stay tuned for more information about this change in Q1.

## To improve the security of the Certificate Transparency (CT) ecosystem, we began dogfooding an [opt-in approach](https://docs.google.com/document/d/1G1Jy8LJgSqJ-B673GnTYIG4b7XRw2ZLtvvSlrqFcl4A/edit) to audit CT information seen in the wild, and we started [designing](https://docs.google.com/document/d/1YTUzoG6BDF1QIxosaQDp2H5IzYY7_fwH8qNJXSVX8OQ/edit#heading=h.7nki9mck5t64) improvements to make this approach more resilient.

## The Chrome Safe Browsing team helped the Chrome for iOS team roll out real-time Safe Browsing protections in Chrome 86 for iOS. Also, in addition to our existing mechanism to disable malicious Chrome Extensions with a large install base, we rolled out a new mechanism that allows us to also disable malware extensions with a small install base.

## On the memory safety front, we've been getting ready to ship [Oilpanned](https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/platform/heap/BlinkGCAPIReference.md) [XFA](https://en.wikipedia.org/wiki/XFA) and continue to engage with the [MiraclePtr and \*Scan](https://docs.google.com/document/d/1pnnOAIz_DMWDI4oIOFoMAqLnf_MZ2GsrJNb_dbQ3ZBg/edit#) project. As those initiatives are treating the symptom rather than the cause, we continue to investigate what a safer dialect of C++ would look like, and to improve Rust/C++ interoperability ahead of any possible future rust experiments. Ongoing work on exploit mitigations includes [Control-flow Enforcement Technology](https://newsroom.intel.com/editorials/intel-cet-answers-call-protect-common-malware-threats/), [GWP-ASan](https://chromium.googlesource.com/chromium/src/+/lkgr/docs/gwp_asan.md), and [Control Flow Guard](https://docs.microsoft.com/en-us/windows/win32/secbp/control-flow-guard).

## We’re also working on reducing the privilege of the network service sandbox on Windows. We’re planning to do the same on Android later in the year.

## [FuzzBench](https://github.com/google/fuzzbench) continues to help the research community benchmark and create more efficient fuzzing engines (e.g. [AFL++ 3.0](https://github.com/AFLplusplus/AFLplusplus/releases/tag/3.0c), [SymQEMU](https://twitter.com/mboehme_/status/1351729922364960770), etc). We added support for [bug-based benchmarking](https://github.com/google/fuzzbench/search?p=1&q=%22type%3A+bug%22) ([sample report](https://www.fuzzbench.com/reports/2020-12-19-bug/index.html)), [fuzzer stats api](https://github.com/google/fuzzbench/pull/648), [saturated corpora testing](https://github.com/google/fuzzbench/pull/760). Our [OSS-Fuzz](https://github.com/google/oss-fuzz) platform now has first-class support for [Python fuzzing](https://google.github.io/oss-fuzz/getting-started/new-project-guide/python-lang/), and continues to grow at a brisk pace ([~400](https://github.com/google/oss-fuzz/tree/master/projects) projects, [25K+](https://bugs.chromium.org/p/oss-fuzz/issues/list?q=-status%3AWontFix%2CDuplicate&can=1) bugs). Based on community feedback, we created a lightweight, standalone [ClusterFuzz python package](https://pypi.org/project/clusterfuzz/) (alpha) for common fuzzing use cases, e.g. stacktrace parsing. We have refactored [AFL fuzzing integration](https://github.com/google/clusterfuzz/pull/2147) to use the [engine interface](https://github.com/google/clusterfuzz/blob/master/src/python/lib/clusterfuzz/fuzz/engine.py). We have been working on a solution to better track vulnerabilities in third-party dependencies. We have also bootstrapped several open source security efforts under the [OpenSSF](https://openssf.org/) foundation, e.g. [security scorecards](https://opensource.googleblog.com/2020/11/security-scorecards-for-open-source.html), [finding critical projects](https://opensource.googleblog.com/2020/12/finding-critical-open-source-projects.html), etc.

## We implemented blocking of requests from insecure contexts to private networks (first part of [CORS-RFC1918](https://web.dev/cors-rfc1918-feedback/)), and are analyzing metrics to chart a path to launch.

## We introduced the [PolicyContainer](https://github.com/antosart/policy-container-explained) to squash bugs around inheritance of security policies to about:blank, srdoc or javascript documents.

## We also implemented a first version of a [Sanitizer API](https://github.com/WICG/sanitizer-api) and started the specification process.

## With [CrossOriginOpenerPolicy](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Cross-Origin-Opener-Policy) (COOP) and [CrossOriginEmbedderPolicy](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Cross-Origin-Embedder-Policy) (COEP) launched, we were able to [re-enable SharedArrayBuffers on Android](https://docs.google.com/document/d/1tXfF0sdMQJPtwc2qEGF_V_z5xiCkP3ayS5ByRz6Rc-A/edit?ts=5f236efa) gated behind crossOriginIsolated (a.k.a COOP+COEP), which Firefox has also done. We have a plan to [deprecate all SAB usage without crossOriginIsolated](https://groups.google.com/a/chromium.org/g/blink-dev/c/1NKvbIj3dq4/m/cdfo-JazBQAJ) in Chrome 91 (with reverse Origin Trial until Chrome 93).

## This will require users of SharedArrayBuffers to adopt COOP and COEP. Adopting COEP has proved difficult. We have heard that the deployment of COEP was difficult for a certain number of websites that embed third-party content. We are considering a new form of COEP that might alleviate those issues: [credentialless](https://github.com/mikewest/credentiallessness). To help drive adoption of COOP we moved the [COOP reporting API](https://web.dev/coop-coep/) out of Origin Trial to on by default in Chrome 89.

## We have started to collect [metrics](https://deprecate.it/) on dangerous web behaviors, with the hope of driving them down. The first one we’ll likely be looking at is [document.domain](https://github.com/mikewest/deprecating-document-domain).

## The Security Architecture team completed the [CORS for content scripts migration](/Home/chromium-security/extension-content-script-fetches) in Chrome 87, removing the allowlist for older extensions and strengthening Site Isolation for all desktop users! Opt-in origin isolation was renamed to [Origin-Keyed Agent Clusters](https://www.chromestatus.com/feature/5683766104162304) and is on track to launch in Chrome 88. We are making progress towards additional Android Site Isolation for OAuth and COOP sites, and we helped secure SkBitmap IPCs against memory bugs. Finally, we have been investing in architecture changes, including [SiteInfo](https://source.chromium.org/chromium/chromium/src/+/HEAD:content/browser/site_instance_impl.h;drc=62f7e7ad10582e60fb724e65dd2b088d4837fe4e;l=28) to better track principals and SiteInstanceGroup to simplify the process model, along with significant reviews for [Multiple Page Architecture](https://docs.google.com/document/d/1NginQ8k0w3znuwTiJ5qjYmBKgZDekvEPC22q0I4swxQ/edit?usp=sharing) and [Multiple Blink Isolates](https://docs.google.com/document/d/1qgDcgQWIXbsJrJUPuqnXv7sy8zf9xrf1ol90D0g7H5o/edit?usp=sharing).

## Cheers,

## Andrew, on behalf of the Chrome security team

## Q3 2020

Greetings,

Here's an update on what the teams in Chrome Security have been up to in the
third quarter of 2020.

The Chrome Safe Browsing team continued the [roll-out of Enhanced Safe
Browsing](https://security.googleblog.com/2020/05/enhanced-safe-browsing-protection-now.html)
by launching it on Android in Chrome 86, and [releasing a
video](https://www.youtube.com/watch?v=w8uNzQqsTrU) with background on the
feature. We also launched [deep scanning of suspicious
downloads](https://security.googleblog.com/2020/09/improved-malware-protection-for-users.html),
initially for users of Google’s Advanced Protection program, which received
[positive
coverage](https://www.theverge.com/2020/9/16/21439599/google-chrome-scan-malicious-files-safe-browsing-advanced-protection).

This quarter the Usable Security team vanquished a longtime foe: http://
subresources on https:// pages. [Mixed
content](https://blog.chromium.org/2019/10/no-more-mixed-messages-about-https.html)
is either upgraded to https:// or blocked. We also built new warnings for [mixed
forms](https://blog.chromium.org/2020/08/protecting-google-chrome-users-from.html)
and continued rolling out [mixed download
blocking](https://blog.chromium.org/2020/02/protecting-users-from-insecure.html).
These launches protect users’ privacy and security by decreasing plaintext
content that attackers can spy on or manipulate.

In Chrome 86, we are beginning a gradual rollout of a new low-confidence warning
for lookalike domains. We also expanded our existing [lookalike
interstitial](https://blog.chromium.org/2019/06/new-chrome-protections-from-deception.html).

Finally, we rolled out a 1% Chrome 86
[experiment](https://blog.chromium.org/2020/08/helping-people-spot-spoofs-url.html)
to explore how simplifying the URL in the address bar can improve security
outcomes.

The Platform Security team continued to move forward on memory safety: With Rust
currently not approved for use in Chromium, we must try to improve C++. Toward
that end, the [PDFium
Oilpan](https://docs.google.com/document/d/1WiZCu0D2RvdpBkYuUdL571oFlj0kSaXU1HhdoL7438Y/edit#heading=h.v9as6odlrky3)
and
[MiraclePtr/\*Scan](https://docs.google.com/document/d/1pnnOAIz_DMWDI4oIOFoMAqLnf_MZ2GsrJNb_dbQ3ZBg/edit)
projects are moving forward quickly and ready to try in Q4 and Q1 2021.

In Sandboxing news, we made changes to Linux and our calling code to handle
coming glibc changes, servicifying the Certificate Verifier (unblocking work to
isolate the network service), and getting a better grip on Mojo.

Bugs-- has started encouraging Chrome developers to submit vulnerability
analysis after the bug is fixed
([example](https://bugs.chromium.org/p/chromium/issues/detail?id=1126424#c41)).
This guides our future work on eliminating common bug patterns. We
cross-collaborated with fuzzing teams across Google to host 50 summer interns,
with strong impact across Chrome and other critical open source software (see
[blog
post](https://opensource.googleblog.com/2020/10/fuzzing-internships-for-open-source.html)).
We have added automated regression testing of past fixed crashes for
engine-based fuzzers (e.g. libFuzzer, AFL). We have made several changes to our
underlying fuzzing and build infrastructures - UI improvements, Syzkaller
support, OSS-Fuzz builder rewrite, etc. Lastly, we continue to push fuzzing
research across the industry using our FuzzBench benchmarking platform and have
led to improvements in
[AFL++](https://github.com/google/fuzzbench/commits?author=vanhauser-thc),
[libFuzzer](https://github.com/google/fuzzbench/commits?author=dokyungs) and
[Honggfuzz](https://github.com/google/fuzzbench/commits?author=robertswiecki)
fuzzing engines.

The Open Web Platform security team continues to focus on two problems:
injection attacks, and isolation primitives.

Regarding injection, we're polishing our [Trusted
Types](https://web.dev/trusted-types/) implementation, supporting Google's
security team with bug fixes as they continue to roll it out across Google
properties. We're following that up with experimental work on a [Sanitizer
API](https://github.com/WICG/sanitizer-api) that's making good progress, and
some hardening work around policy inheritance to fix a class of bugs that have
cropped up recently.

For isolation, we're continuing to focus on
[COOP](https://web.dev/why-coop-coep/#coop) deployment. We shipped COOP's
report-only mode as an origin trial, and we're aiming to re-enable
SharedArrayBuffers behind COOP+COEP in Chrome 88 after shipping some changes to
the process model in Chrome 87 to enable \`crossOriginIsolated\`.

In Q3, Chrome's Security Architecture team has enabled CORS for extension
content scripts in Chrome 85, moving to a more secure model against compromised
renderers. We made further progress on opt-in origin isolation, and we took the
first steps towards several improved process model abstractions for Chrome.
MiraclePtr work is progressing towards experiments, and we wrapped up the test
infrastructure improvements from last quarter.

The CA/Browser Forum guidelines got big updates, with ballots to overhaul the
guidelines to [better match browser
requirements](https://github.com/cabforum/documents/pull/195), including
certificate lifetimes, and long overdue [cleanups and
clarifications](https://github.com/cabforum/documents/pull/208). One good revamp
deserves another, and the [Chrome Root Certificate
Policy](https://g.co/chrome/root-policy) got a big facelift, as part of
transitioning to a Chrome Root Store.

CT Days 2020 [was held in
September](https://docs.google.com/document/d/18hRJQW5Qzgcb87P-YIkWIuna9_pRUNGsZPC4V0fiq5w/preview),
including the big announcement that Chrome was working to remove the One Google
Log requirement by implementing [SCT
auditing](https://docs.google.com/document/d/1G1Jy8LJgSqJ-B673GnTYIG4b7XRw2ZLtvvSlrqFcl4A/edit).

[This
summer](https://security.googleblog.com/2020/10/fuzzing-internships-for-open-source.html),
we also hosted an intern who worked on [structure-aware ASN.1
fuzzing](https://github.com/google/libprotobuf-mutator-asn1), and began
integration with BoringSSL.

Cheers,

Andrew, on behalf of the Chrome security team

## Q2 2020

Greetings,

The 2nd quarter of 2020 saw Chrome Security make good progress on multiple
fronts, all helping to keep our users, and the web safe.

The Chrome Safe Browsing team launched real-time phishing protection for all
Android devices, and observed a 164% increase in phishing warnings for
main-frame URLs. We also completed the rollout of [Enhanced Safe
Browsing](https://security.googleblog.com/2020/05/enhanced-safe-browsing-protection-now.html)
to all users of Chrome on desktop platforms.

We helped the Chrome for iOS team implement hash-based Safe Browsing protection
in Chrome 84 for iOS for the first time ever. Also working with various teams,
most notably the Mobile UX, we made significant progress in shipping Enhanced
Safe Browsing in Chrome 86 for Android.

For desktop platforms, we landed changes to the in-browser phishing detection
mechanism to help reduce phishing false negatives using new machine learning
models for Chrome 84 and beyond. We also finalized the plan to disable more
malicious Chrome Extensions, starting with Chrome 85.

The Enamel team put the finishing touches on our work to prevent https:// pages
from loading insecure content. We built a new warning for https:// pages with
forms targeting insecure endpoints, and prepared to start rolling out [mixed
download
warnings](https://blog.chromium.org/2020/02/protecting-users-from-insecure.html)
in Chrome 84. This release will also include [mixed image
autoupgrading](https://security.googleblog.com/2019/10/no-more-mixed-messages-about-https_3.html)
and the second phase of [TLS 1.0/1.1
deprecation](https://security.googleblog.com/2019/10/chrome-ui-for-deprecating-legacy-tls.html).

Even on an https:// website, users need to accurately understand which website
they’re visiting. We expanded our [lookalike domain
warning](https://blog.chromium.org/2019/06/new-chrome-protections-from-deception.html)
with new triggering heuristics, and prepared to launch an additional warning
(pictured) for lower-precision heuristics in M86.

The Platform Security team continued to make good progress on many of our longer
term projects, including sandboxing the network service (and associated
certificate verification servicification),
[adopting](https://docs.google.com/document/d/1WiZCu0D2RvdpBkYuUdL571oFlj0kSaXU1HhdoL7438Y/)
Oilpan garbage collection in PDFium's XFA implementation, and investigating
memory safety techniques, and exploitation mitigation technologies.

Along with our colleagues in Chrome Security Architecture, we've sharpened the
security focus on
[Mojo](https://chromium.googlesource.com/chromium/src/+/HEAD/mojo/README.md),
Chrome's IPC system, and started looking at what's needed to improve developer
ergonomics and make it easier to reason about communicating over security
boundaries. Also with CSA, we've worked on how
[MiraclePtr](https://docs.google.com/document/d/1pnnOAIz_DMWDI4oIOFoMAqLnf_MZ2GsrJNb_dbQ3ZBg/edit?usp=sharing)
could help prevent use after free bugs in C++ code.

Bugs-- continued to develop and improve the FuzzBench platform which has helped
the security research community develop more efficient fuzzing engines
(HonggFuzz, AFL++ got several improvements and leads the [benchmarking
results](https://www.fuzzbench.com/reports/2020-07-13/index.html)). Based on
FuzzBench results, we have successfully integrated
[Entropic](https://mboehme.github.io/paper/FSE20.Entropy.pdf) as a fuzzing
strategy in ClusterFuzz. We have started rewriting/improving several Chrome
blackbox fuzzers (e.g. dom, webbot, media, ipc), and also deprecated ~50
duplicate/unneeded fuzzers. In OSS-Fuzz service, we added first-class fuzzing
support for
[Golang](https://google.github.io/oss-fuzz/getting-started/new-project-guide/go-lang/)
and
[Rust](https://google.github.io/oss-fuzz/getting-started/new-project-guide/rust-lang/)
languages (better compiler instrumentation, crash parsing, and easier project
integration) and improved CI (e.g. Honggfuzz checks). Lastly, we worked closely
with Android Security and improved ClusterFuzz for on-device and host fuzzing
use cases (e.g. syzkaller support, pixel hardware fuzzing).

The Open Web Platform Security team remained focused on mitigating injection
attacks on the one hand, and improving isolation of sensitive content on the
other. Q2 was exciting on both fronts!

We shipped an initial implementation of [Trusted
Types](https://web.dev/trusted-types/), which gives developers the ability to
meaningfully combat DOM XSS, and nicely compliments CSP's existing mitigations
against other forms of injection. Google has deployed Trusted Types in
high-value applications like [My Google
Activity](https://myactivity.google.com/), and we're excited about further
rollouts.
We also rolled out our first pass at [two new isolation
primitives](https://web.dev/coop-coep): Cross-Origin Opener Policy and
Cross-Origin Embedder Policy. Opting-into these mechanisms improves our ability
to process-isolate your pages, mitigating some impacts of Spectre and XSLeaks,
which makes it possible to safely expose powerful APIs like SharedArrayBuffers.

The Chrome Security Architecture team has started Origin Trials for [opt-in
origin isolation](https://www.chromestatus.com/feature/5683766104162304),
allowing origins to use separate processes from the rest of their site. We have
also made progress on [securing extension content script
requests](/Home/chromium-security/extension-content-script-fetches) and
enforcements for request initiators, and we improved the update mechanism for
Android Site Isolation's list of isolated sites. Much of Q2 was spent on cleanup
and documentation, though, particularly test infrastructure and flaky test
improvements. Finally, we also contributed to
[MiraclePtr](https://docs.google.com/document/d/1pnnOAIz_DMWDI4oIOFoMAqLnf_MZ2GsrJNb_dbQ3ZBg/edit?usp=sharing)
efforts to reduce memory bugs, and we helped more teams use WebUI by adding
support for web iframes.

In the world of the Web PKI, TLS certificates issued from default-trusted CAs
after 2020-09-01 will be rejected if their lifetime is greater than 398 days,
beginning with Chrome 85. See the [documentation and
FAQ](https://chromium.googlesource.com/chromium/src/+/HEAD/net/docs/certificate_lifetimes.md).
This is part of a number of changes [adopted
by](https://cabforum.org/2020/07/16/ballot-sc31-browser-alignment/) CA/Browser
Forum with unanimous support from major Browsers, which aligns the Baseline
Requirements with many existing Browser root program requirements.

We continued informal cross-browser collaboration and met with the European
Union on their eIDAS Regulation, exploring how certificates can be used to
provide identity information for domains in a manner consistent with the Web
Platform.

Until next time, on behalf of Chrome Security, I wish you all the very best.

Andrew

## Q1 2020

Greetings,

Amongst everything the first quarter of 2020 has thrown at the world, it has
underlined the crucial role the web plays in our lives. As always, the Chrome
Security teams have been focusing on the safety of our users, and on keeping
Chrome secure and stable for all those who depend on it.

The Chrome Safe Browsing team, with the support of many teams, introduced a new
Safe Browsing mode that users can opt-in to get “faster, proactive protection
against dangerous websites, downloads, and extensions.”

We launched [previously
announced](https://www.blog.google/products/chrome/better-password-protections/)
faster phishing protection to Chrome users on high-memory Android devices. This
led to a 116% increase in the number of phishing warnings shown to users for
main frame URLs.

We also launched predictive phishing protections to all users of Chrome Password
Manager on Android, which warns users when they type their saved password on an
unsafe website. The initial estimate from the launch on Beta population suggests
an 11% increase in the number of warnings shown compared to that on Windows.

Chrome's Enamel team finalized plans to bring users a more secure HTTPS
ecosystem by blocking [mixed
content](https://security.googleblog.com/2019/10/no-more-mixed-messages-about-https_3.html),
[mixed
downloads](https://blog.chromium.org/2020/02/protecting-users-from-insecure.html),
and [legacy TLS
versions](https://security.googleblog.com/2019/10/chrome-ui-for-deprecating-legacy-tls.html).
These changes have now been delayed due to changing global circumstances, but
are still planned for release at the appropriate time.

To improve how users understand website identity, we experimented with a [new
security indicator
icon](https://bugs.chromium.org/p/chromium/issues/detail?id=1008219) for
insecure pages. We also experimentally launched a [new
warning](https://bugs.chromium.org/p/chromium/issues/detail?id=982930) for sites
with spoofy-looking domain names. We’re now analyzing experiment results and
planning next steps for these changes.

The Platform Security team made significant forward progress on enabling the
network service to be sandboxed on all platforms (it already is on macOS). This
required getting significant changes into Android R, migrating to a new way of
using the Data Protection API on Windows (which had the side-effect of [breaking
some crime rings’
operations](https://www.zdnet.com/article/chrome-80-update-cripples-top-cybercrime-marketplace/),
albeit
[temporarily](https://www.bleepingcomputer.com/news/security/malware-unfazed-by-google-chromes-new-password-cookie-encryption/)),
and more. When complete, this will reduce the severity of bugs in that service
from Critical to High.

We also made progress on Windows sandboxing, working towards adopting
AppContainer, and are refactoring our Linux/Chrome OS sandbox to handle
disruptive upstream changes in glibc and the kernel.

Discussions about the various ways we can improve memory safety continue, and we
laid plans to migrate PDFium’s XFA support to
[Oilpan](https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/platform/heap/BlinkGCAPIReference.md)
garbage collection, with the help of Oilpan and V8 teams. This will enable us to
safely ship XFA in production, hopefully in 2020.

The bugs-- team launched [FuzzBench](https://github.com/google/fuzzbench), a
fuzzer benchmarking platform to bridge the gap between academic fuzzing research
and industry fuzzing engines (e.g libFuzzer, AFL, Honggfuzz). We have integrated
new techniques in ClusterFuzz to improve fuzzing efficiency and break coverage
walls - dataflow trace based fuzzing, in-process grammar mutators (radamsa,
peach). Also, launched
[CIFuzz](https://google.github.io/oss-fuzz/getting-started/continuous-integration/)
for OSS-Fuzz projects to catch obvious security regressions in a project’s
continuous integration before they are checked in.

The Chrome Security Architecture (née Site Isolation) team has been
strengthening Site Isolation this quarter. We're [securing extension content
script requests](/Home/chromium-security/extension-content-script-fetches) to
unify CORS and CORB behavior, and we're progressing with a
[prototype](https://crbug.com/1042415) to let websites opt in to [origin-level
isolation](https://github.com/WICG/origin-isolation). To improve Chrome's
security architecture, the team is working on a proposal for a new
SecurityPrincipal abstraction. We have also cleaned up RenderWidget/RenderView
lifetimes. Finally, we are starting to formalize our thinking about privilege
levels and their interactions in Chrome. We are enumerating problem spots in IPC
and other areas as we plan the next large projects for the team.

For the past five years, Chrome, along with counterparts at browser vendors such
as Mozilla, Microsoft, Apple, Opera, and Vivaldi, have been discussing technical
challenges involved in [the eIDAS
Regulation](https://ec.europa.eu/futurium/en/content/eidas-regulation-regulation-eu-ndeg9102014)
with members of the European Commission, [ETSI](https://www.etsi.org/), and
[European Union Agency for Cybersecurity (ENISA)](https://www.enisa.europa.eu/).
These discussions saw more activity this past quarter, with browsers [publicly
sharing](https://cabforum.org/pipermail/servercert-wg/2020-January/001555.html)
an [alternative technical
proposal](https://cabforum.org/pipermail/servercert-wg/attachments/20200114/3a5fa74c/attachment-0001.pdf)
to the current ETSI-defined approach, in order to help the Commission make the
technology easier to use and interoperate with the web and browsers.

We announced [Chrome’s 2020 Certificate Transparency
plans](https://groups.google.com/a/chromium.org/d/msg/ct-policy/dqFtoFBy8YU/Xa67FWVCEgAJ)
with a focus on removing “One Google Log” policy dependency. Pending updates to
travel policy, we have tentatively planned CT Days 2020 and sent out an
[interest
survey](https://docs.google.com/forms/d/1NzHadfwhqUPEXqcHHtZTwfVDS1yj9vGvhP0y1qluYEU/edit)
for participants.

Until next time, on behalf of Chrome Security I wish you all the very best.

Andrew

## Q4 2019

As we start 2020 and look forward to a new year and a [new
decade](https://xkcd.com/2249/), the Chrome Security Team took a moment to look
back at the final quarter of 2019.

The Safe Browsing team launched two features that significantly improve phishing
protections available to Chrome users:

We reduced the false negative rate for Safe Browsing lookups in Chrome by
launching [real-time Safe Browsing
lookups](https://security.googleblog.com/2019/12/better-password-protections-in-chrome.html)
for users who have opted in to “Make Searches and Browsing better.” Early
results are promising, with up to [55% more warnings
shown](https://screenshot.googleplex.com/EADPM3soc10) to users who had this
protection turned on, compared to those who did not.

A while ago we launched predictive phishing protections to warn users who are
syncing history in Chrome when they enter their Google Account password into
suspected phishing sites that try to steal their credentials. [With the Chrome
79](https://security.googleblog.com/2019/12/better-password-protections-in-chrome.html),
we expanded this protection to everyone signed in to Chrome, even if you have
not enabled Sync. In addition, this feature will now work for all the passwords
that the user has stored in Chrome’s password manager; this will show an
estimated 10 times more warnings daily.

We also had two telemetry based launches for sending pings to Safe Browsing when
users who have opted into Safe Browsing Extended Reporting focus on password
fields and reuse their passwords on Android.

[HTTPS adoption](https://transparencyreport.google.com/https/overview?hl=en) has
risen dramatically, but
[many](https://chromestatus.com/metrics/feature/timeline/popularity/609)
https:// pages still include http:// subresources — known as mixed content. In
October, the Usable Security team published a
[plan](https://security.googleblog.com/2019/10/no-more-mixed-messages-about-https_3.html)
to eradicate mixed content from the web. The first phases of this plan started
shipping in Chrome 79. In Chrome 79, we relocated the setting that allows users
to load mixed content when it’s blocked by default. This setting used to be a
shield icon in the Omnibox, and is now available in Site Settings instead. In
Chrome 80, mixed audio and video will be automatically upgraded to https://, and
they will be blocked if they fail to load. We started work on a [web
standard](https://w3c.github.io/webappsec-mixed-content/level2.html) to codify
these changes. See [this
article](https://developers.google.com/web/fundamentals/security/prevent-mixed-content/fixing-mixed-content)
for how to fix mixed content if you run an affected website.

Website owners should keep their HTTPS configurations up-to-date with the latest
security settings. Back in 2018, we (alongside other browsers) announced
[plans](https://security.googleblog.com/2018/10/modernizing-transport-security.html)
to remove support for legacy TLS versions 1.0 and 1.1. In October, we updated
these plans to
[announce](https://security.googleblog.com/2019/10/chrome-ui-for-deprecating-legacy-tls.html)
the specific UI treatments that we’ll use for this deprecation. Starting in
January 2020, Chrome 79 will label affected websites with a “Not Secure” chip in
the omnibox. Chrome 81 will show a full-page error. Make sure your server
supports TLS &gt;=1.2 to avoid this warning treatment.

To continue to polish our security UI, we iterated on our
[warning](https://blog.chromium.org/2019/06/new-chrome-protections-from-deception.html)
for lookalike domains to make the warning more understandable. We introduced a
new gray triangle icon for http:// sites to make a clearer distinction between
http:// and https://. This icon will appear for some users as part of a
small-scale experiment in Chrome 80. Finally, we cleaned up a large backlog of
low severity security UI vulnerabilities. We fixed, closed, or removed
visibility restrictions on 33 out of 42 bugs.

The Platform Security Team sandboxed the network service on macOS in Chrome 79,
and continued the work on sandboxing it on other Desktop platforms. There is
also some forward momentum for reducing its privilege in version R of Android.

You can now check the sandboxing state of processes on Windows by navigating to
chrome://sandbox. Also on Windows, we experimented with enabling the renderer
App Container but ran into crashes likely related to third party software, and
are now working to improve error reporting to support future experimentation.
Chrome 79 also saw Code Integrity Guard enabled on supported Windows versions,
blocking unsigned code injection into the renderer process.

We have also begun investigating new systemic approaches to memory unsafety.
Look for news in 2020, as well as continual improvements to the core libraries
in Chromium and PDFium.

In Q4, the Bugs-- team moved closer to our goal of achieving 50% fuzzing
coverage in Chrome (it's currently at 48%). We added new features to our
ClusterFuzz platform, such as [Honggfuzz](https://github.com/google/honggfuzz)
support, libFuzzer support for Android, improved fuzzer weights and more
accurate statistics gathering pipeline. We also enabled several new UBSan
features across both Chrome and OSS-Fuzz. As part of OSS-Fuzz, we added Go
language support and on-boarded several new Go projects. We also gave a talk
about ClusterFuzz platform at [Black Hat
Europe](https://www.blackhat.com/eu-19/briefings/schedule/#clusterfuzz-fuzzing-at-google-scale-17505).

In conversation with our friends and colleagues at Mozilla over the course of
Q4, the Open Web Platform Security team made substantial progress on
[Cross-Origin-Opener-Policy](https://github.com/whatwg/html/issues/3740) and
[Cross-Origin-Embedder-Policy](https://mikewest.github.io/corpp/). These
isolation primitives will make it possible for us to ensure that process
isolation is robust, even as we ship new and exciting APIs that give developers
more capability. Implementations of both are mostly complete behind a flag, and
we're looking forward to getting them out the door, and beginning the process of
relying upon them to [when deciding whether to allow cross-thread access to
shared
memory](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer/Planned_changes).

Similarly, we're polishing our implementation of [Trusted
Types](https://w3c.github.io/webappsec-trusted-types/dist/spec/) based on
feedback from origin trials and other vendors' review of the spec. We're still
excited about its potential for injection mitigation, and we're looking forward
to closing out the last few issues we know about in our implementation.

The Site Isolation team posted to the [Google Security
Blog](https://security.googleblog.com/2019/10/improving-site-isolation-for-stronger.html)
and the [Chromium
Blog](https://blog.chromium.org/2019/10/recent-site-isolation-improvements.html)
about our recent milestones for Site Isolation on Android and defending against
compromised renderer processes. We also gave a talk at [Black Hat
Europe](https://www.blackhat.com/eu-19/briefings/schedule/#site-isolation-confining-untrustworthy-code-in-the-web-browser-17974)
about Site Isolation and how to look for new bypasses for the
[VRP](https://www.google.com/about/appsecurity/chrome-rewards/index.html). At
the same time, we made progress on additional enforcement, and we ran
experiments to expand Android coverage to more devices. Finally, we also used Q4
to clean up a lot of core Site Isolation code, and we started updating Chrome's
WebUI framework to better support new types of Chrome features without large
risks of privilege escalation.

In the world of Web PKI Security, as part of our ongoing collaboration with
Microsoft and Mozilla on the [Common CA Database](https://www.ccadb.org/),
"Audit Letter Validation" is now enabled for the full set of publicly trusted
Certificate Authorities. This tool, developed by Microsoft and Mozilla,
automatically validates the contents of audit letters to ensure they include the
information required of a publicly trusted CA. Audit letter validation was
previously done by hand, which was not scalable to CA's 2,500+ intermediate
certificates.

Audit Letter Validation enabled us and other root stores to detect a wide
variety of issues in the Web PKI that had previously gone unnoticed. We’ve spent
the past quarter leading the incident response effort, working with
non-compliant CAs to remediate issues and mitigate future risk. This helps not
only Chrome users, but all users who trust these CAs. We can now automatically
detect issues as they happen, ensuring prompt remediation.

We also collaborate with Mozilla to provide [detailed
reviews](https://wiki.mozilla.org/CA/Dashboard#Detailed_CP.2FCPS_Review) of
organizations applying to be CAs, completing several in Q4. These public reviews
take an extremely detailed look at how the CA is operated, looking at both
compliance and for risky behaviour not explicitly forbidden, as well as
opportunities for improvement based on emerging good practices.

[Certificate Transparency](https://www.certificate-transparency.org/) (CT)
continues to be an integral part of our work. Beyond helping protect users by
allowing quick detection of potentially malicious certificates, the large-scale
analysis that CT enables has been essential in helping improve the Web PKI.
Analysis of CT logs this quarter revealed a number of systemic flaws in how
Extended Validation certificates are validated, which has spurred industry-wide
effort to address these issues.

We took steps to protect users from trusting harmful certificates that might be
installed by software or which they might be directed to install. Working with
the Enamel team, we built on steps we’d [previously taken to protect
users](https://security.googleblog.com/2019/08/protecting-chrome-users-in-kazakhstan.html)
from certificates used to intercept their communications by adding the ability
to rapidly deploy targeted protections via our
[CRLSet](/Home/chromium-security/crlsets) mechanism. CRLSets allow us to quickly
respond, using the [Component
Updater](https://chromium.googlesource.com/chromium/src/+/HEAD/components/component_updater/README.md),
without requiring a full Chrome release or respin.

More generally, we continue to work on the “patch gap”, where security bug fixes
are posted in our open-source code repository but then take some time before
they are released as a Chrome stable update. We now make regular refresh
releases every two weeks, containing the latest severe security fixes. This has
brought down the median “patch gap” from 33 days in Chrome 76 to 15 days in
Chrome 78, and we continue to work on improving it.

Finally, you can read what the Chrome (and other Google) Vulnerability Rewards
Programs have been up to in 2019 in our [recent blog
post](https://security.googleblog.com/2020/01/vulnerability-reward-program-2019-year.html).

Cheers,

Andrew, on behalf of the Chrome security team

## Q3 2019

Greetings!

With the equinox behind us, it's time for an update on what the Chrome security
team has been up to in the third quarter of 2019.

The Chrome Safe Browsing team
[launched](https://www.blog.google/technology/safety-security/advanced-protection-program-expands-chrome/)
Stricter Download Protections for [Advanced
Protection](https://landing.google.com/advancedprotection/) users in Chrome and
significantly reduce users’ exposure to potentially risky downloads.

In Q3, Safe Browsing also brought Google password protection to signed in,
non-sync users. This project is code complete, and the team plans to roll it out
in Chrome 79.

Enamel, the Security UX team, have been looking at mixed content: http://
subresources on https:// pages. Mixed content presents a confusing UX and a risk
to user security and privacy. After a long-running data-gathering
[experiment](https://groups.google.com/a/chromium.org/d/msg/blink-dev/ZJxkCJq5zo4/4sSMVZzBAwAJ)
on pre-stable channels, the Enamel team
[publicized](https://security.googleblog.com/2019/10/no-more-mixed-messages-about-https_3.html)
plans to start gradually blocking mixed content. In Chrome 79, the team plans to
relocate the setting to bypass mixed content blocking from a shield icon in the
omnibox to Site Settings. In Chrome 80, we will start auto-upgrading mixed audio
and video to https://, blocking resources if they fail to auto-upgrade. Chrome
80 will also introduce a “Not Secure” omnibox chip for mixed images, which we
plan to start auto-upgrading in a future version of Chrome.

Furthering our quest to improve the quality of HTTPS deployments, we
[announced](https://security.googleblog.com/2019/10/chrome-ui-for-deprecating-legacy-tls.html)
a new UI plan for the upcoming [legacy TLS
deprecation](https://security.googleblog.com/2018/10/modernizing-transport-security.html)
in early 2020.

In Q3, Enamel also made improvements to our lookalike domain warning, with
clearer strings and new heuristics for detecting spoofing attacks. We also added
additional signals in our [Suspicious Site Reporter
extension](https://chrome.google.com/webstore/detail/suspicious-site-reporter/jknemblkbdhdcpllfgbfekkdciegfboi?hl=en-US)
for power users to identify suspicious sites that they can report to Safe
Browsing for scanning. In Chrome 77, we
[relocated](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/ev-to-page-info.md)
the Extended Validation certificate UI to Page Info; we presented the user
research that inspired this change at [USENIX Security
2019](https://www.usenix.org/conference/usenixsecurity19/presentation/thompson).

The Platform Security team continues to help improve the memory safety of the
PDFium code base, and have finished removing all bare new/delete pairs, and
ad-hoc refcounting. We continued to push for greater [memory
safety](https://twitter.com/arw/status/1159631867835895808) on a number of
fronts, and are busy working on plans for the rest of the year and 2020. Q3 saw
a number of projects enter trials on Beta and Stable, including the V2 sandbox
for GPU process and network service sandbox on macOS, and Code Integrity Guard
on Windows. Look out for news of their launch in next quarter's update!

The [XSS Auditor](/developers/design-documents/xss-auditor), which attempted to
detect and prevent reflected XSS attacks, was [removed in Chrome
78](https://www.zdnet.com/article/google-to-remove-chromes-built-in-xss-protection-xss-auditor/).
It had [a number of
issues](https://groups.google.com/a/chromium.org/forum/#!msg/blink-dev/TuYw-EZhO9g/blGViehIAwAJ),
and in the end the cons outweighed the pros.

The Bugs-- team added
[FuzzedDataProvider](https://github.com/google/fuzzing/blob/master/docs/split-inputs.md#fuzzed-data-provider)
(FDP) as part of Clang, making it simple to write fuzz targets that require
multiple inputs with just a single header file include. We refactored
ClusterFuzz code to make it easier to add new fuzzing engines and migrated
libFuzzer to use this new
[interface](https://github.com/google/clusterfuzz/blob/master/src/python/bot/fuzzers/engine.py).
We rewrote the ClusterFuzz reproduce tool, which is now part of main ClusterFuzz
GitHub repo. On the OSS front, we launched new features in OSS-Fuzz - Golang
support, X86 config support, FDP support, and OSS-Fuzz Badges. We also did
fuzzer strategy weight adjustments based on multi-armed bandit experiments.
Jonathan Metzman
[presented](https://www.blackhat.com/us-19/briefings/schedule/index.html#going-beyond-coverage-guided-fuzzing-with-structured-fuzzing-16110)
at Black Hat (USA) on structure aware fuzzing.

The Open Web Platform Security team have been working on [Trusted
Types](https://github.com/w3c/webappsec-trusted-types), the Origin Trial for
which is about to finish. We are making a number of changes to the feature,
mainly to aid deployment and debugging of TT deployments, as well as some
overall simplifications. We expect this work to finish in early Q4, and to
launch in the same quarter.

The [Site Isolation](/Home/chromium-security/site-isolation) team reached two
more important milestones in Q3. First, we enabled Site Isolation for password
sites on Chrome for Android (on devices with at least 2GB of memory), bringing
Spectre mitigations to mobile devices! Second, we added enough compromised
renderer protections on Chrome for Desktop to include cross-site data disclosure
to the [Chrome
VRP](https://www.google.com/about/appsecurity/chrome-rewards/index.html)! We're
very excited about the new protections, and we continue to improve the defenses
on both Android and Desktop. Separately, we [presented our USENIX Security
paper](https://www.usenix.org/conference/usenixsecurity19/presentation/reis) in
August and launched OOPIF-based PDF support, clearing the way to remove
BrowserPlugin.

In the Web PKI space, the government of Kazakhstan recently created a Root CA
and with local ISPs engaged in a campaign to encourage all KZ citizens to
install and trust the CA. Ripe Atlas detected this CA [conducting a
man-in-the-middle](https://censoredplanet.org/kazakhstan) on social media.
Chrome blocked this certificate to prevent it from being used for MITMing Chrome
users. In conjunction with several other major browsers, we made a [joint PR
statement](https://blog.mozilla.org/blog/2019/08/21/mozilla-takes-action-to-protect-users-in-kazakhstan/)
against this type of intentional exploitation of users. Following this incident,
we began working on a long-term solution to handling MITM CAs in Chrome.

In hacker philanthropy news, in July we increased the amounts awarded to
security researchers who submit security bugs to us under the [Chrome
Vulnerability Reward Program](https://g.co/ChromeBugRewards). The update aligned
both categories and amounts with the areas we'd like researchers to focus on.
This generated some good [press
coverage](https://techcrunch.com/2019/07/18/google-will-now-pay-bigger-rewards-for-discovering-chrome-security-bugs/)
which should help spread the word about the Chrome VRP. Tell your friends, and
submit your Chrome security bugs
[here](https://bugs.chromium.org/p/chromium/issues/entry?template=Security%20Bug)
and they'll be considered for a reward when they're fixed!

In Chrome security generally we've been working to address an issue called the
“patch gap”, where security bug fixes are posted in our open-source code
repository but then take some time before they are released as a Chrome stable
update. During that time, adversaries can use those fixes as evidence of
vulnerabilities in the current version of Chrome. To reduce this problem, we’ve
been merging more security fixes directly to stable, and we’re now always making
a security respin mid-way through the six-week development cycle. This has
reduced the median patch gap from ~33 days in Chrome 76 to ~19 days in Chrome
77. This is still too long, and we’re continuing to explore further solutions.

Cheers,

Andrew, on behalf of the Chrome security team

## Q2 2019

Greetings,

With 2019 already more than 58% behind us, here's an update on what Chrome
Security was up to in the second quarter of this year.

Chrome SafeBrowsing is launching stricter download protections for [Advanced
Protection](https://landing.google.com/advancedprotection/) users, and a
teamfood has begun to test the policy in M75. This will launch broadly with M76.
This significantly reduces an Advanced Protection user’s exposure to potentially
risky downloads by showing them warnings when they try to download “risky” files
(executable files that haven’t been vetted by SafeBrowsing) in Chrome.

Users need to understand site identity to make safe decisions on the web. Chrome
Security UX published a [USENIX Security
paper](https://ai.google/research/pubs/pub48199) exploring how users understand
modern browser identity indicators. To help users understand site identity from
confusing URLs, we launched a [new
warning](https://security.googleblog.com/2019/06/new-chrome-protections-from-deception.html)
detecting domains that look similar to domains you’ve visited in the past. We
published a
[guide](https://docs.google.com/document/d/1_xJz3J9kkAPwk3pma6K3X12SyPTyyaJDSCxTfF8Y5sU/edit)
to how we triage spoofing bugs involving such domains. We also built a
[Suspicious Site Reporter
extension](https://chrome.google.com/webstore/detail/suspicious-site-reporter/jknemblkbdhdcpllfgbfekkdciegfboi)
that power users can use to report deceptive sites to Google’s Safe Browsing
service, to help protect non-technical users who might not be able to discern a
deceptive site’s identity as well.

Site identity is meaningless without HTTPS, and we continue to promote HTTPS
adoption across the web. We implemented an experimental flag to
[block](https://groups.google.com/a/chromium.org/d/msg/blink-dev/mALJa0JM13I/-jxMlOyrBAAJ)
high-risk nonsecure downloads initiated from secure contexts. And we continued
to roll out our experiment that auto-upgrades mixed content to HTTPS, pushing to
10% of beta channel and adding new metrics to quantify breakage.

In addition to helping with the usual unfaltering flow of security launch
reviews, Platform Security engineers have been continuing to investigate ways to
help Chrome engineers create fewer memory safety bugs for clusterfuzz to find.
While performance is a concern when adding checks to libraries, some reports of
regressions nicely turned out to be [red
herrings](https://bugs.chromium.org/p/chromium/issues/detail?id=957296#c15). On
macOS, Chrome executables are [now
signed](https://chromium-review.googlesource.com/c/chromium/src/+/1666734) with
the [hardened runtime
options](https://developer.apple.com/documentation/security/hardened_runtime_entitlements)
enabled. Also on macOS, the change to have [Mojo use Mach
IPC](https://docs.google.com/document/d/1nEUEAP3pq6T9WgEucy2j02QwEHi0PaFdpaQcT8ooH9E/edit#),
rather than POSIX file descriptors/socket pairs, is now fully rolled out. On
Windows, we started to
[enable](https://bugs.chromium.org/p/chromium/issues/detail?id=961831)
[Arbitrary Code
Guard](https://docs.microsoft.com/en-us/windows/security/threat-protection/windows-defender-exploit-guard/exploit-protection-exploit-guard#mitigation-comparison)
on processes that don't need dynamic code at runtime.

We've done a lot of analysis on the types of security bugs which are still
common in Chromium. The conclusion is that memory safety is still our biggest
problem, so we've been working to figure out the best next steps to solve
that—both in terms of safer C++, and investigating other choices to find if we
can parse data in a safe language without disrupting the Chromium development
environment too much.

We've also been looking at how security fixes are released, to ensure fixes get
to our users in the quickest possible way. We have also improved some of the
automatic triage that Clusterfuzz does to make sure that bugs get the right
priority.

To augment our fuzzing efforts and find vulnerabilities for known bad patterns,
we have decided to invest in static code analysis efforts with
[Semmle](https://semmle.com/). We have written our custom QL queries and
reported
[15](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=label%3AFound-With-Semmle)
bugs so far (some of these were developed in collaboration with [Project
Zero](https://googleprojectzero.blogspot.com/)).

We have made several changes to improve fuzzing efficiency which include -
leveraging [DFSan](https://clang.llvm.org/docs/DataFlowSanitizer.html) for
[focused mutations](https://github.com/google/clusterfuzz/issues/503), added
support for [custom mutators](https://github.com/google/clusterfuzz/pull/286),
build-type optimizations (sanitizers without instrumentation) and libFuzzer fork
mode on Windows. We have
[upstreamed](https://cs.chromium.org/chromium/src/third_party/libFuzzer/src/utils/FuzzedDataProvider.h)
a helper module in libFuzzer to make it easy to split fuzz input and decrease
fuzz target complexity.

The Open Web Platform Security team was mainly focused on [Trusted
Types](https://github.com/WICG/trusted-types), and conducted an Origin Trial for
the feature in Q2. The team is presently scrambling to address the issues raised
by public feedback, to modify the feature to make it easier to deploy, and to
generally make Trusted Types fit for a full launch.

The [Site Isolation](/Home/chromium-security/site-isolation) team published
their Usenix Security 2019 paper about the desktop launch ([Site Isolation:
Process Separation for Web Sites within the
Browser](https://ai.google/research/pubs/pub48285)), which will be presented in
August. We now have a small Stable channel trial of Android Site Isolation,
which isolates the sites that users log into rather than all sites. That work
included persisting and clearing the sites to isolate, fixing text autosizing,
and adding more metrics. Separately, we ran a trial of isolating origins rather
than sites to gauge overhead, and we helped ship [Sec-Fetch-Site
headers](https://w3c.github.io/webappsec-fetch-metadata/). We also started
collecting data on how well CORB is protecting sensitive resources in practice,
and we've started launch trials of out-of-process iframe based PDFs (which adds
CORB protection for PDFs).

The Chrome OS Security team has been working on the technology underlying Chrome
OS verified boot. Going forward, dm_verity will use SHA256 as its hashing
algorithm, replacing SHA1. So long, weak hashing algorithm!

We also spent some time making life easier for Chrome OS developers. Devs now
have access to a
[time-of-check-time-of-use](https://en.wikipedia.org/wiki/Time-of-check_to_time-of-use)
safe [file
library](https://chromium.googlesource.com/chromiumos/platform2/libbrillo/+/HEAD/brillo/safe_fd.h),
and a [simplified
mechanism](https://groups.google.com/a/chromium.org/d/msg/chromium-os-dev/zYP4tlXQmRg/aMyd2l-SBAAJ)
for building system call filtering policies.

Cheers,

Andrew, on behalf of the Chrome security team

## Q1 2019

Greetings,

Here's an update on what Chrome Security was up to in the first quarter of 2019!

The [Site Isolation](/Home/chromium-security/site-isolation) team finished the
groundwork for Android Beta Channel field trials, and the trials are now in
progress. This Android mode isolates a subset of sites that users log into, to
protect site data with less overhead than isolating all sites. We also started
enforcing Cross-Origin Read Blocking for [extension content script
requests](/Home/chromium-security/extension-content-script-fetches), maintaining
a temporary allowlist for affected extensions that need to migrate. We tightened
compromised renderer checks for navigations, postMessage, and BroadcastChannel.
We also continued cross-browser discussions about [Long-Term Web Browser
Mitigations for
Spectre](https://docs.google.com/document/d/1dnUjxfGWnvhQEIyCZb0F2LmCZ9gio6ogu2rhMGqi6gY/edit?usp=sharing),
as well as headers for [isolating
pages](https://github.com/whatwg/html/issues/3740) and [enabling precise
timers](https://github.com/whatwg/html/issues/4175). Finally, we are close to
migrating PDFs from BrowserPlugin to out-of-process iframes, allowing
BrowserPlugin to be deleted.

In the last several years, the Usable Security team have put a lot of effort
into [improving HTTPS
adoption](https://transparencyreport.google.com/https/overview) across the web,
focusing on getting top sites to migrate to HTTPS for their top-level resources.
We’re now starting to turn our attention to insecure subresources, which can
harm user security and privacy even if the top-level page load is secure. We are
currently running an
[experiment](https://chromestatus.com/feature/5557268741357568) on Canary, Dev,
and Beta that automatically upgrades insecure subresources on secure pages to
HTTPS. We also collected metrics on insecure downloads in Q1 and have started
putting together a
[proposal](https://lists.w3.org/Archives/Public/public-webappsec/2019Apr/0004.html)
to block high-risk insecure downloads initiated from secure pages.

People need to understand website identity to make good security and trust
decisions, but
[lots](http://people.ischool.berkeley.edu/~tygar/papers/Phishing/why_phishing_works.pdf)
[of](http://www.usablesecurity.org/papers/jackson.pdf)
[research](http://grouplab.cpsc.ucalgary.ca/grouplab/uploads/Publications/Publications/2011-DomainHighlighting.CHI.pdf)
suggests that they don’t. We summarized our own research and thinking on this
topic in an [Enigma 2019 talk](https://www.youtube.com/watch?v=RPoAc0ScdTM). We
open-sourced a [tool](https://github.com/chromium/trickuri) that we use to help
browser developers display site identity correctly. We also published a set of
[URL display
guidelines](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/url_display_guidelines/url_display_guidelines.md)
and subsequently incorporated them into the [URL
standard](https://url.spec.whatwg.org/#url-rendering).

The Safe Browsing team increased the coverage against malware and unwanted
software downloads by changing the logic of which file types to check against
Safe Browsing. We flipped the heuristic to an allow-list of known-safe file
extensions, and made the rest require verification. This adds protection from
both the [uncommon
](https://chromium-review.googlesource.com/c/chromium/src/+/1459317)file
extensions (where attackers convince users to rename them to a common executable
after scanning), and from [Office document
types](https://chromium-review.googlesource.com/c/chromium/src/+/1449110) where
the incidence of malware has increased significantly.

The Chrome Cleanup Tool is now in the Chromium repository! This lets the public
audit the data collected by the tool, which is a win for user privacy, and gives
an example of how to sandbox a file scanner. The open source version includes a
sample scanner that detects only test files, while the version shipped in Chrome
will continue to depend on internal resources for a licensed engine.

The Bugs-- team has [open sourced](https://github.com/google/clusterfuzz)
ClusterFuzz, a fuzzing infrastructure that we have been developing over the past
8 years! This army of robots has found 30,000+ bugs in Chrome and 200+ open
source projects. To improve the efficiency of our cores, we have developed
automated fuzzer weights management based on fuzzer quality/freshness/code
changes. Additionally, we have developed several new WebGL fuzzers (some of them
leverage [GraphicsFuzz](https://github.com/google/graphicsfuzz)) and found
[63](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=metzman_graphicsfuzz_crash_fuzzer+-status%3AWontFix%2CDuplicate+OR+metzman_graphicsfuzz_mutator++-status%3AWontFix%2CDuplicate+OR+metzman_webgl_api_fuzzer++-status%3AWontFix%2CDuplicate+OR+metzman_webgl_mutator++-status%3AWontFix%2CDuplicate)
bugs. We have significantly scaled up fuzzing Chrome on Android (x86) by using
[Cuttlefish](https://github.com/google/android-cuttlefish) over
[GCE](https://cloud.google.com/compute/docs/). Lastly, we have transitioned
Chrome code coverage tools development to Chrome Infra team, see the new dash
[here](https://analysis.chromium.org/p/chromium/coverage).

The Platform Security team added some checks for basic safety to our base and
other fundamental libraries, and are investigating how to do more while
maintaining efficiency (run-time, run space, and object code size). We hope to
continue to do more, as well as investigate how to use absl without forgoing the
safety checks. We’ve been having great success with this kind of thing in PDFium
as well, where we’ve found that the compiler can often optimize away these
checks, and investigating where it hasn’t been able to has highlighted several
pre-existing bugs. On macOS, we have re-implemented the Mojo IPC Channel under
the hood to use Mach IPC, which should help reduce system resource shortage
crashes. This also led to the development of two libprotobuf-mutator (LPM)
fuzzers for Mach IPC servers. We’re working on auto-generating an LPM based
fuzzer from Mojo API descriptions to automatically fuzz Mojo endpoints,
in-process. We also continue to write LPM fuzzers for tricky-to-reach areas of
the code like the disk cache. We are also investigating reducing the privilege
of the network process on Windows and macOS.

Our next update will be the first full quarter after joining Chrome Trust and
Safety. We're looking forward to collaborating with more teams who are also
working to keep our users safe!

Cheers,

Andrew on behalf of Chrome Security

## Q4 2018

Greetings,

With the new year well underway, here's a look back at what Chrome Security was
up to in the last quarter of 2018.

In our quest to make HTTPS the default, we started marking HTTP sites with a red
Not Secure icon when users enter data into forms. This change launched to stable
in Chrome 70 in October. A new version of the HTTPS error page also launched to
the stable channel as an experiment: it looks the same but is much improved
[under the
hood](https://docs.google.com/document/d/1rEBpw5V-Nn1UIi8CIFa5ZZvwlR08SkY3CogvWE2UMFs/edit).
We built a new version of the [HTTPS Transparency
Report](https://transparencyreport.google.com/https/overview?hl=en) for top
sites; the report now displays aggregate statistics for the top sites instead of
individual sites. We also built a [new interstitial
warning](https://blog.chromium.org/2018/11/notifying-users-of-unclear-subscription.html)
to notify Chrome users of unclear mobile subscription billing pages. The new
warning and policy launched in Chrome 71.

The Bugs-- team ported libFuzzer to work on Windows, which was previously
lacking coverage guided fuzzing support, and this resulted in
[93](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=windows_libfuzzer_chrome_asan+reporter%3Aclusterfuzz%40chromium.org+-status%3Aduplicate%2Cwontfix&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids)
new bugs. We hosted a month-long
[Fuzzathon](https://groups.google.com/a/chromium.org/d/msg/chromium-dev/MAiBRTllPuI/hPbEMRWQDAAJ)
in November, focused on improving fuzz coverage for Chrome’s browser process and
Chrome OS. This effort led to 85 submissions and
[157](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=-status%3AWontFix%2CDuplicate+id%3A907387%2C908754%2C903899%2C907386%2C904054%2C911112%2C904053%2C906711%2C912230%2C906370%2C907662%2C907663%2C910842%2C910843%2C906416%2C906417%2C906418%2C907302%2C903724%2C906395%2C906393%2C906391%2C911475%2C906399%2C908049%2C901782%2C907912%2C908196%2C906007%2C908829%2C907847%2C905334%2C912202%2C910918%2C912208%2C910917%2C904613%2C906568%2C906374%2C907561%2C907560%2C912455%2C907070%2C903251%2C910852%2C910851%2C903252%2C906396%2C910480%2C903828%2C911030%2C906349%2C908678%2C903052%2C903782%2C912219%2C902693%2C902690%2C904689%2C904682%2C905273%2C905272%2C905275%2C907999%2C911320%2C906352%2C906350%2C906356%2C906354%2C910497%2C910970%2C906359%2C904055%2C905649%2C910930%2C905401%2C906469%2C906705%2C910522%2C907693%2C906462%2C910835%2C902964%2C907051%2C906466%2C906372%2C903772%2C912299%2C911155%2C910926%2C910929%2C910928%2C906659%2C903233%2C907345%2C907344%2C904093%2C904090%2C905413%2C906329%2C909713%2C908781%2C905985%2C909801%2C912506%2C906801%2C901649%2C904382%2C905259%2C902605%2C908039%2C903280%2C904141%2C908004%2C910898%2C902131%2C910069%2C904655%2C910896%2C903088%2C906337%2C906440%2C912476%2C906333%2C908392%2C902227%2C912479%2C906448%2C906339%2C910248%2C904712%2C904792%2C910469%2C903690%2C901239%2C902860%2C911700%2C906381%2C906382%2C911822%2C905621%2C904725%2C910866%2C910862%2C911827%2C910860%2C907524%2C901675%2C901674%2C906438%2C906439%2C910892%2C907278%2C904736%2C904734%2C907718%2C907157%2C910592%2C908237%2C907453%2C908232%2C904105%2C908230%2C912227%2C912224%2C912223%2C904221%2C905463%2C903237%2C903236%2C904227%2C903234%2C906425%2C906421%2C906423%2C906422%2C906429%2C906428%2C908209%2C911409%2C912520&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids)
bugs. We have added more automation towards auto-adjusting cpu cycles allocated
to various fuzzers based on code coverage changes and recency of fuzzer
submission. Lastly, we added Linux x86 fuzzing configurations
([1](https://ci.chromium.org/p/chromium/builders/luci.chromium.ci/Libfuzzer%20Upload%20Linux32%20ASan),
[2](https://ci.chromium.org/p/chromium/builders/luci.chromium.ci/Libfuzzer%20Upload%20Linux32%20ASan%20Debug))
for libFuzzer, which resulted in
[100](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=x86_libfuzzer_chrome_asan+-status%3AWontFix%2CDuplicate+OR+x86_libfuzzer_chrome_asan_debug+-status%3AWontFix%2CDuplicate&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids)
new bugs.

In Platform Security, we started sandboxing the network service on macOS. On
Windows, we’re starting to experiment with an improved GPU sandbox. The network
service has the beginnings of a sandbox on Windows, and we’ll be working on
tightening it in future work. We’re also continuing to gradually harden the
implementations of core Chromium libraries in base/ and elsewhere. We had a
great adventure finding and fixing bugs in SQLite as well, including an
innovative and [productive new
fuzzer](https://github.com/google/fuzzer-test-suite/blob/master/tutorial/structure-aware-fuzzing.md#example-sqlite).
We’re continuing to hammer away at bugs in PDFium, and refactoring it
significantly.

To help sites defend against cross-site scripting (XSS), we are working on
[Trusted Types](https://wicg.github.io/trusted-types/dist/spec/). This aims to
bring a derivative of Google's "[Safe HTML
Types](https://github.com/google/safe-html-types/blob/master/doc/safehtml-types.md)"
— which relies on external tooling that may be incompatible with existing
workflows or code base — directly into the web platform, thus making it
available to everyone. Both Google-internal and [external
teams](https://github.com/cure53/DOMPurify/blob/master/demos/trusted-types-demo.html)
are presently working on integrating Trusted Types into existing frameworks
which, if successful, offers the chance to rapidly bring this technique to large
parts of the web. Chrome 73 will see an [origin
trial](https://groups.google.com/a/chromium.org/forum/#!msg/blink-dev/I9To21DXcLo/NrU9P0M4EAAJ).

The work on [Site Isolation](/Home/chromium-security/site-isolation) continues
as we focus on enabling it on Android — support for adding isolated origins at
runtime, fixing issues with touch events, and balancing process usage for
maximizing stability. We added improvements to CORB to prevent bypasses from
exploited renderers, we announced [extensions changes for content script
requests](/Home/chromium-security/extension-content-script-fetches), and we
reached out to affected authors with guidance on how to update. Additionally, we
continue to add more enforcements to mitigate compromised renderers, which is
the ultimate end goal of the project. Last but not least, we have worked to
improve code quality and clean up architectural deficiencies which accumulated
while developing the project.

Chrome OS 71 saw the initial, limited release of USBGuard, a technology that
improves the security of the Chrome OS lock screen by (carefully) blocking USB
devices on the lock screen.

As ever, many thanks to all those in the Chromium community, and our VRP
reporters, who help make the Web more secure!

Cheers,

Andrew, on behalf of the Chrome security team

## Q3 2018

Greetings!

Chrome turned 10 in September! Congrats to the team on a [decade of making the
web more
secure](https://www.wired.com/story/chrome-decade-making-the-web-more-secure/).

In the quest to find security bugs, the Bugs-- team incorporated Machine
Learning in ClusterFuzz infrastructure using [RNN
model](https://en.wikipedia.org/wiki/Recurrent_neural_network) to improve upon
corpus quality and code coverage. We experimented with improving fuzzing
efficiency by adding instability handling and mutation stats strategies inside
libFuzzer. We added a new [Mojo service
fuzzer](https://bugs.chromium.org/p/chromium/issues/detail?id=607649) by
extending the Mojo javascript bindings and found [security
bugs](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=mojo_fuzzer+label%3AClusterFuzz+Type%3DBug-Security&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids).
We also
[migrated](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/code_coverage.md)
our fuzzing infrastructure to provide [Clang Source-based Code
Coverage](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html) reports and
deprecated [Sancov](https://clang.llvm.org/docs/SanitizerCoverage.html).

The Platform Security team continued to add hardening and checks to fundamental
classes and libraries in base/, and did some of the same work in PDFium and
other parsers and interpreters in Chromium. We also provided some sandboxing
consulting to other teams for their new services including audio and networking.

Chrome on macOS now has a new sandbox architecture, launched in Chrome 69, which
immediately initializes when a new process executes. This reduces Chrome’s
attack surface and allows better auditing of system resource access between
macOS versions.

Chrome OS Security wrapped up the response to the [L1TF
vulnerability](https://www.intel.com/content/www/us/en/architecture-and-technology/l1tf.html),
fixes for which enabled shipping Linux apps on Chrome OS without exposing users
to extra risk. Moreover, we received [an (almost) full-chain exploit for Chrome
OS](https://bugs.chromium.org/p/chromium/issues/detail?id=884511) that both
validated earlier sandboxing work (like for Shill, Chrome OS’s connection
manager) and also shed light on further hardening work that was wrapped up in
Q3.

Chrome 70 shipped TLS 1.3, although we did have to disable a downgrade check in
this release due to a last-minute incompatibility with some network devices.

After the excitement [enabling Site Isolation by default on desktop
platforms](https://security.googleblog.com/2018/07/mitigating-spectre-with-site-isolation.html)
in Q2, the team has been focused on building a form of Site Isolation suitable
for devices that run Android, which have more limited memory and processing
power. We've been fixing Android-specific issues (alongside a lot of maintenance
for the desktop launch), we have started field trials for isolating a subset of
sites, and we are working on ways to add more sites to isolate at runtime.
Separately, we added several more enforcements to mitigate compromised
renderers, to extend the protection beyond Spectre.

Users should expect that the web is safe by default, and they’ll be warned when
there’s an issue. In Chrome 68, we hit a milestone for Chrome security UX,
[marking all HTTP
sites](https://www.blog.google/products/chrome/milestone-chrome-security-marking-http-not-secure/)
as “not secure”. We continued down that path in Chrome 70, [showing the “not
secure” string in
red](https://www.blog.google/products/chrome/milestone-chrome-security-marking-http-not-secure/)
when users enter data on an HTTP page. We began stepping towards removing
Chrome’s positive security indicators so that the default unmarked state is
secure, starting by [removing the “Secure”
wording](https://blog.chromium.org/2018/05/evolving-chromes-security-indicators.html)
in Chrome 69.

We [would like to
experiment](https://groups.google.com/a/chromium.org/forum/#!msg/blink-dev/ZJxkCJq5zo4/4sSMVZzBAwAJ)
with mixed content autoupgrading to simplify (i.e. improve) the user experience,
and are currently collecting metrics about the impact. We’re also working to
improve Chrome security UX under the hood -- we launched [committed HTTPS
interstitials](https://docs.google.com/document/d/1rEBpw5V-Nn1UIi8CIFa5ZZvwlR08SkY3CogvWE2UMFs/edit)
on Canary and Dev.

As ever, many thanks to all those in the Chromium community, and our[ VRP
reporters](https://www.google.com/about/appsecurity/chrome-rewards/index.html),
who help make the Web more secure!

Cheers,

Andrew, on behalf of the Chrome security team

## Q2 2018

Greetings and salutations,

It's time for another (rather belated!) update from your friends in Chrome
Security, who are hard at work to keep Chrome the most secure platform to browse
the Internet.

We're very excited that [Site Isolation](/Home/chromium-security/site-isolation)
is now [enabled by
default](https://security.googleblog.com/2018/07/mitigating-spectre-with-site-isolation.html)
as a Spectre mitigation in M67 for Windows, macOS, Linux, and Chrome OS users!
This involved an incredible number of fixes from the team in Q2 to make
out-of-process iframes fully functional, especially in areas like painting,
input events and performance, and it included standardizing [Cross-Origin Read
Blocking (CORB)](/Home/chromium-security/corb-for-developers). Stay tuned for
more updates on Site Isolation coming later this year, including additional
protections from compromised renderers. Chris and Emily talked about Spectre
response, Site Isolation, and necessary developer steps [at
I/O](https://youtu.be/dBuykrdhK-A). We also announced that security bugs found
in Site Isolation could qualify for [higher VRP reward
payments](https://www.google.com/about/appsecurity/chrome-rewards/index.html#special)
for a limited time.

In their quest to find security bugs, the Bugs-- team integrated [Clang
Source-based Code
Coverage](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html) into
Chromium project and launched a
[dashboard](https://chromium-coverage.appspot.com/) to make it easy for
developers to see which parts of the code are not covered by fuzzers and unit
tests. We wrote a [Mojo service
fuzzer](https://cs.chromium.org/chromium/src/mojo/public/tools/fuzzers/) that
generates fuzzing bindings in JS and found some scary
[vulnerabilities](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=mojo_fuzzer+Type%3DBug-Security&sort=-type&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified+Type&x=m&y=releaseblock&cells=ids).
We added [libFuzzer fuzzing support in Chrome
OS](https://chromium.googlesource.com/chromiumos/docs/+/HEAD/fuzzing.md) and
got new fuzz target contributions from Chrome OS developers and found several
[bugs](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=libfuzzer_asan_chromeos+-status%3ADuplicate%2CWontFix+label%3AClusterFuzz&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids).
We made numerous improvements to our ClusterFuzz fuzzing infrastructure,
examples include dynamically adjusting CPU allocation for inefficient fuzz
targets until their performance issues are resolved, cross-pollinating corpuses
across fuzz targets and projects, and more.

The Platform Security team has been working on adding bounds checks and other
sanity checks to
[base/containers](https://bugs.chromium.org/p/chromium/issues/detail?id=817982),
as part of an overarching effort to harden heavily-used code and catch bugs.
We’ve had some good initial success and expect to keep working on this for the
rest of the year. This is a good area for open source contributors and VRP
hunters to work on, too!

In our quest to move the web to 100% HTTPS, we prepared for showing Not Secure
warnings on all http:// pages which started in M68. We sent Search Console
messages to affected sites and expanded our [enterprise
controls](/administrators/policy-list-3#UnsafelyTreatInsecureOriginAsSecure) for
this warning. We
[announced](https://blog.chromium.org/2018/05/evolving-chromes-security-indicators.html)
some further changes to Chrome’s connection security indicators: in M69, we’ll
be removing the Secure chip next to https:// sites, and in M70 we’ll be turning
the Not Secure warning red to more aggressively warn users when they enter data
on a non-secure page.

We also added some features to help users and developers use HTTPS more often.
The omnibox now remembers pages that redirect from http:// to https://, so that
users don’t get sent to the http:// version in the future. We fixed a
longstanding bug with the
[upgrade-insecure-requests](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Upgrade-Insecure-Requests)
CSP directive that helps developers find and fix mixed content: it now upgrades
requests when following redirects. Finally, we added a setting to
chrome://flags#unsafely-treat-insecure-origin-as-secure to let developers more
easily test HTTPS-only features, especially on Android and ChromeOS.

To better protect users from unwanted extensions, we announced [the deprecation
of inline installations for
extensions](https://blog.chromium.org/2018/06/improving-extension-transparency-for.html).
This change will result in Chrome users being directed to the Chrome Web Store
when installing extensions, helping to ensure user can make a better informed
decision.

Chrome OS spent a big chunk of Q2 updating and documenting our processes to
ensure we can better handle future incidents like Spectre and Meltdown. We
expanded our [security review
guidelines](https://chromium.googlesource.com/chromiumos/docs/+/HEAD/security_review_howto.md)
so that they can be used both by security engineers while reviewing a feature,
as well as by SWE and PM feature owners as they navigate the Chrome OS launch
process.

We continued our system hardening efforts by making Shill, the Chrome OS network
connection manager, run in a restrictive, non-root environment starting with
M69. [Shill was exploited as part of a Chrome OS full-chain
exploit](https://googleprojectzero.blogspot.com/2016/12/chrome-os-exploit-one-byte-overflow-and.html),
so sandboxing it was something that we’ve been wanting to do for a long time.
With PIN sign-in launching with M68, the remaining work to make the underlying
user credential brute force protection mechanism more robust is underway, and we
plan to enable it for password authentication later this year. Hardening work
also happened on the Android side, as we made progress on functionality that
will allow us to verify generated code on Android using the TPM.

Q2 continued to require incident response work on the Chrome OS front, as the
fallout from Spectre and Meltdown included several researchers looking into the
consequences of speculative execution. The good news is that we started
receiving updated microcode for Intel devices and these updates will start to go
out with M69.

As ever, many thanks to all those in the Chromium community, and our [VRP
reporters](https://www.google.com/about/appsecurity/chrome-rewards/index.html),
who help make the Web more secure!

Cheers

Andrew

on behalf of the Chrome Security Team

## Q1 2018

**Greetings and salutations,**

It's time for another update from your friends in Chrome Security, who are hard at work trying to keep Chrome as the most secure platform to browse the Internet. We'd also like to welcome our colleagues in Chrome OS security to this update - you'll be able to hear what they've been up to each quarter going forward.**

In our effort to find and fix bugs, we collaborated with the [Skia](https://skia.org/) team and [integrated](https://github.com/google/oss-fuzz/tree/master/projects/skia) 21 fuzz targets into OSS-Fuzz for continuous 24x7 fuzzing on Skia trunk. So far, we have found [38](https://bugs.chromium.org/p/oss-fuzz/issues/list?can=1&q=Proj%3Dskia+Type%3DBug-Security+-status%3ADuplicate&colspec=ID+Type+Component+Status+Proj+Reported+Owner+Summary&cells=ids) security vulns! We also added several new fuzz targets as part of a 2-week bug bash (e.g. [multi-msg mojo fuzzer](https://chromium-review.googlesource.com/c/chromium/src/+/973685), [audio decoder fuzzer](https://chromium-review.googlesource.com/c/chromium/src/+/976184), [appcache manifest parsing fuzzer](https://chromium-review.googlesource.com/c/chromium/src/+/982677), [json fuzzer](https://chromium-review.googlesource.com/c/chromium/src/+/959564) [improvements](https://chromium-review.googlesource.com/c/chromium/src/+/971063), etc) and found an additional [vulnerability](https://crbug.com/826193) through code review. We added libFuzzer support for Chrome OS and integrated it with ClusterFuzz. Sample [puffin fuzzer](https://chromium-review.googlesource.com/c/chromiumos/overlays/chromiumos-overlay/+/944190) found [11](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=libfuzzer_asan_chromeos+-status%3AWontFix%2CDuplicate&colspec=ID+Type+Component+Status+Proj+Reported+Owner+Summary&x=m&y=releaseblock&cells=ids) bugs (includes 2 security). We made several improvements to AFL fuzzing engine integration and fuzzing strategies. This brings it on-par with libFuzzer in terms of the number of bugs found -- it's now ~3X more productive than before! We added support for building MSan instrumented system libraries for newer debian distros ([1](https://github.com/google/oss-fuzz/issues/608), [2](/developers/testing/memorysanitizer)).**

To [help users infected with unwanted software](https://support.google.com/chrome/answer/2765944?co=GENIE.Platform%3DDesktop&hl=en), we moved the standalone Chrome Cleanup Tool into Chrome. Scanning and cleaning Windows machines can now be triggered by visiting chrome://settings/cleanup. There was some misunderstanding on Twitter about why Chrome was scanning, which we clarified. We also pointed people to the [unwanted software protection](https://www.google.com/chrome/privacy/whitepaper.html#unwantedsoftware) section of Chrome's privacy whitepaper so they can understand what data is and isn’t sent back to Google.**

In our effort to move the web to 100% HTTPS, we [announced](https://security.googleblog.com/2018/02/a-secure-web-is-here-to-stay.html) that Chrome will start marking all HTTP pages with a Not Secure warning in July. This is a big milestone that concludes a multi-year effort to roll out this warning to all non-secure pages. Alongside that announcement, we added a [mixed content audit](https://developers.google.com/web/tools/lighthouse/audits/mixed-content) to [Lighthouse](https://developers.google.com/web/tools/lighthouse/), an automated tool for improving webpage quality. This audit helps developers find and fix mixed content, a major hurdle for migrating to HTTPS. We also [announced](https://groups.google.com/a/chromium.org/forum/#!msg/blink-dev/ANnafFBhReY/1Xdr53KxBAAJ) the deprecation of AppCache in nonsecure contexts.**

In addition to MOAR TLS, we also want more secure and usable HTTPS, or BETTER TLS. With that goal in mind, we made changes to get better metrics about features intended to help users with client or network misconfigurations that break their HTTPS connections (like our [customized certificate warnings](https://research.google.com/pubs/archive/46359.pdf)). We also added more of these “helper” features too: for example, we now bundle help content targeted at users who are stuck with incorrect clocks, captive portals, or other configuration problems that interfere with HTTPS. Finally, we started preparing for Chrome’s upcoming Certificate Transparency [enforcement deadline](https://groups.google.com/a/chromium.org/d/msg/ct-policy/wHILiYf31DE/iMFmpMEkAQAJ) by analyzing and releasing some [metrics](https://www.youtube.com/watch?v=e_rwG7MA5VU) about the state of CT adoption so far.**

To help make security more usable in Chrome, we’re exploring how [URLs are problematic](https://www.youtube.com/watch?v=UD-ukjVoeLc). We removed https/http schemes and www/m subdomains from the steady-state omnibox, and we’re studying the impact of removing positive security indicators that might mislead or distract from the important security information in the origin.**

**Chrome OS Security had a busy Q1. The vulnerabilities known as [Meltdown and
Spectre](https://meltdownattack.com/) were disclosed in early January, and a
flurry of activity followed as we rushed to patch older kernels against Meltdown
in Chrome OS 66, and incorporated Spectre fixes for ARM Chrome OS devices in
Chrome OS 67. We also started codifying our [security review guidelines in a
HOWTO
doc](https://chromium.googlesource.com/chromiumos/docs/+/HEAD/security_review_howto.md),
to allow the larger Chrome OS team to better prepare for security reviews of
their features. Moreover, after being bit by symlinks and FIFOs being used as
part of [several](https://bugs.chromium.org/p/chromium/issues/detail?id=344051)
[exploit](https://bugs.chromium.org/p/chromium/issues/detail?id=648971)
[chains](https://bugs.chromium.org/p/chromium/issues/detail?id=766253), we
finally landed
[symlink](https://chromium-review.googlesource.com/c/chromiumos/platform2/+/966683)
and
[FIFO](https://chromium-review.googlesource.com/c/chromiumos/platform2/+/978780)
blocking in Chrome OS 67. On the hardware-backed security front, we've split off
the component that allows irreversible once-per-boot decisions into its own
service, bootlockboxd. Finally, work is nearing completion for a first shipping
version of a** hardware-backed mechanism to protect user credentials against
brute force attacks**. This will allow PIN codes as a new authentication
mechanism for Chrome OS meeting our** authentication security guidelines**, and
we'll use it to upgrade password-based authentication to a higher security bar
subsequently.**

**Spectre kept us busy on the Chrome Browser side as well. The V8 team landed a large number of JIT mitigations to make Spectre exploits harder to produce, and high resolution timers like SharedArrayBuffer were temporarily disabled; more details on our response [here](/Home/chromium-security/ssca). In parallel, the [Site Isolation](/Home/chromium-security/site-isolation) team significantly ramped up efforts to get Site Isolation launched as a Spectre mitigation, since it helps avoid having data worth stealing anywhere in a compromised process. In Q1, we substantially improved support for the Site Isolation enterprise policies that launched prior to the Spectre disclosure, including:**

    **Reducing total memory overhead from 20% to 10%.**

    **Significantly improving input event handling in out-of-process iframes
    (OOPIFs).**

    **Significantly improving DevTools support for OOPIFs.**

    **Adding ChromeDriver support for OOPIFs.**

    **Adding support for printing OOPIFs.**

    **Improving rendering performance for OOPIFs.**

    **Starting standards discussions for Cross-Origin Read Blocking (CORB).**

**Thanks to these improvements, we have been running field trials and are preparing to launch the strict Site Isolation policy on desktop. We [talked about](https://www.youtube.com/watch?v=dBuykrdhK-A) much of this work at Google I/O.**

**Finally, we continue to work on exploit mitigations and other security hardening efforts. For example, [Oilpan](https://chromium.googlesource.com/chromium/src/+/lkcr/third_party/WebKit/Source/platform/heap/BlinkGCDesign.md), blink's garbage collecting memory management system, [removed its inline metadata](https://bugs.chromium.org/p/chromium/issues/detail?id=633030), which make it more difficult to overwrite with memory corruption bugs. This was the culmination of several years of effort, as performance issues were worked through. In Android P, we refactored the WebView [zygote](https://developer.android.com/topic/performance/memory-overview) to become a child of the main app_process zygote, reducing memory usage and helping with the performance of future Site Isolation efforts. Members of Platform Security also helped coordinate the response to Spectre and Meltdown, and still managed to find time to conduct their routine reviews of new [Chrome features](https://www.chromestatus.com/features).**

## Q4 2017

Greetings and salutations,

It's time for another update from your friends in Chrome Security, who are hard
at work trying to make Chrome the[ most secure platform to browse the
Internet](/Home/chromium-security). As it's the start of 2018, [we
reflected](https://www.blog.google/products/chrome/reflecting-years-worth-chrome-security-improvements/)
on a year’s worth of security improvements, and announced new stats around our
VRP and Safe Browsing warnings.

Here are some highlights from the last quarter of 2017:

In effort to find and fix bugs, we (Bugs--):

    Wrote a new and easily extensible javascript fuzzer using
    [Babel](https://babeljs.io/), which found 100+ bugs in both V8
    ([list](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=ochang_js_fuzzer+-status%3ADuplicate%2CWontFix&sort=-security_severity+-secseverity+-owner+-modified&colspec=ID+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner+Reporter&x=m&y=releaseblock&cells=tiles))
    and other browser javascript engines
    ([list](https://bugs.chromium.org/p/oss-fuzz/issues/list?can=1&q=js_fuzzer+-status%3AWontFix%2CDuplicate&colspec=ID+Type+Component+Status+Proj+Reported+Owner+Summary&cells=ids)).

    Started integrating [Clang Source-based Code
    Coverage](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html) in the
    Chromium build system and are deprecating [Sanitizer
    Coverage](https://clang.llvm.org/docs/SanitizerCoverage.html). Clang
    coverage is very precise, shows hit frequencies and is much easier to
    visualize. You can follow progress
    [here](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=component%3ATools%3ECodeCoverage).

    Hosted a month long fuzzathon in October where Chromium developers
    participated in writing new [fuzz
    targets](https://chromium.googlesource.com/chromium/src/testing/libfuzzer/+/HEAD/README.md)
    and fixing blockers for existing ones. This resulted in 93 bugs, several of
    which were in new uncovered areas of codebase; results
    [here](https://clusterfuzz.com/v2/fuzzathon).

    Fixed several
    [bugs](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=component%3ATools%3ETest%3EPredator+status%3AFixed%2CVerified&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids)
    in our automated owner and component assignment pipeline and
    [expanded](https://bugs.chromium.org/p/chromium/issues/detail?id=760607) our
    builder infrastructure to archive builds more frequently, for more accurate
    blame results. Faster and more accurate bug triaging means faster fixes for
    users!

Other than fixing bugs, we (MOAR TLS, Enamel, Safe Browsing) also:

    [Blogged](https://www.blog.google/topics/safety-security/say-yes-https-chrome-secures-web-one-site-time/)
    about the massive uptick in HTTPS we saw in 2017: 71 of the top 100 sites on
    the Web use HTTPS by default, up from 37 a year ago. We also announced our
    continuing platinum sponsorship of Let’s Encrypt.

    Delivered a change (in M62, [announced in
    April](https://blog.chromium.org/2017/04/next-steps-toward-more-connection.html)),
    extending the “Not Secure” omnibox warning chip to non-secure pages loaded
    while Incognito and to non-secure pages after the user edits a form field.

    Added an [enterprise
    policy](/administrators/policy-list-3#UnsafelyTreatInsecureOriginAsSecure)
    (in M65) for treating insecure origins as secure contexts, to help with
    development, testing, and intranet sites that are not secured with HTTPS.

    More tightly integrated Chrome’s captive portal detection with operating
    system APIs. This feature helps users log in to captive portals (like hotel
    or airport wifi networks) rather than seeing unhelpful TLS certificate
    errors.

    [Launched](https://www.blog.google/topics/safety-security/new-security-protections-tailored-you/)
    predictive phishing protection to warn users when they’ve typed their Google
    password into a never-seen-before phishing site.

As always, we invest a lot in security architecture and exploit mitigations.
Last quarter, we (Platform Security / Site Isolation):

    Started rolling out the [Mac Sandbox
    v2](https://chromium.googlesource.com/chromium/src/+/HEAD/sandbox/mac/seatbelt_sandbox_design.md),
    bringing both greater security and cleaner code.

    [Refactored sandbox code out of
    //content](https://bugs.chromium.org/p/chromium/issues/detail?id=708738) to
    make it easier to use across the system.

    Worked on and helped coordinate Chrome's response to the recently announced
    [Spectre and Meltdown CPU
    vulnerabilities](https://blog.google/topics/google-cloud/what-google-cloud-g-suite-and-chrome-customers-need-know-about-industry-wide-cpu-vulnerability/)
    and worked with the [V8 team](https://developers.google.com/v8/) who
    spearheaded Chrome's Javascript and WebAssembly mitigations which are
    rolling out to users now.

    Accelerated the rollout of [Site
    Isolation](/Home/chromium-security/site-isolation) as a
    [mitigation](/Home/chromium-security/ssca) for
    [Spectre/Meltdown](https://blog.google/topics/google-cloud/what-google-cloud-g-suite-and-chrome-customers-need-know-about-industry-wide-cpu-vulnerability/).
    Enabling Site Isolation reduces the amount of valuable cross-site data that
    can be stolen by such attacks. We're working to fix the currently known
    issues so that we can start enabling it by default.

    Implemented [cross-site document
    blocking](http://www.chromium.org/developers/design-documents/blocking-cross-site-documents)
    in (M63) when Site Isolation is enabled. This ensures that cross-site HTML,
    XML, JSON, and plain text files are not given to a renderer process on
    subresource requests unless allowed by [CORS](https://www.w3.org/TR/cors/).
    Both --site-per-process and --isolate-origins modes are [now available via
    enterprise
    policy](https://www.blog.google/topics/connected-workspaces/security-enhancements-and-more-enterprise-chrome-browser-customers/)
    in Chrome 63.

    Ran field trials of --site-per-process and --isolate-origins on 50% of
    Chrome Canary instances to measure performance, fix crashes, and spot
    potential issues. In a separate launch involving out-of-process iframes and
    Site Isolation logic, <https://accounts.google.com> now has a dedicated
    renderer process in Chrome 63 to support upcoming requirements for Chrome
    Signin.

    We have landed a large number of functional and performance improvements for
    Site Isolation, including fixes for input events, DevTools, OAuth, hosted
    apps, crashes, and same-site process consolidation which reduces memory
    overhead.

To help users that inadvertently installs unwanted software, we (Chrome
Protector):

    Launched new Chrome Cleanup Tool UI , which we think is more comprehensible
    for users.

    Launched a sandboxed ESET-Powered [Chrome Cleanup
    Tool](https://blog.google/products/chrome/cleaner-safer-web-chrome-cleanup/)

        Running on 100% of Chrome users by Nov 23

Lastly, we (BoringSSL) deployed TLS 1.3 to Chrome stable for a couple weeks in
December and gathered [valuable
data](https://www.ietf.org/mail-archive/web/tls/current/msg25168.html).

As ever, many thanks to all those in the Chromium community who help make the
Web more secure!

Cheers

Andrew

on behalf of the Chrome Security Team

## Q3 2017

Greetings and salutations,

It's time for another update from your friends in Chrome Security, who are hard
at work trying to make Chrome the[ most secure platform to browse the
Internet](/Home/chromium-security). Give you're reading this, you might well be
interested in [two
whitepapers](https://www.blog.google/topics/connected-workspaces/2-new-white-papers-examine-enterprise-web-browser-security/)
evaluating enterprise browser security that were released recently.

Beyond that, here's a recap from last quarter:

Bugs-- team

    We've been researching ways to fuzz grammar based formats efficiently. We
    experimented with in-process fuzzing with
    [libprotobuf-mutator](https://github.com/google/libprotobuf-mutator) and
    added a
    [sample](https://chromium.googlesource.com/chromium/src/+/HEAD/testing/libfuzzer/libprotobuf-mutator.md).

    Recent ClusterFuzz improvements for developers:

        The [reproduce tool](https://github.com/google/clusterfuzz-tools) is now
        out of beta and supports Linux and Android platforms.

        Performance improvements to ClusterFuzz UI and migrated to [Polymer
        2](https://www.polymer-project.org/2.0/docs/about_20).

        We finished the remaining pieces of our end-to-end bug triage automation
        and are now auto-assigning
        [owners](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=label%3ATest-Predator-AutoOwner&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids)
        and
        [components](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=label%3ATest-Predator-AutoComponents&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids)
        for all newly filed bugs.

    We have also made infrastructure improvements to
    [OSS-Fuzz](https://github.com/google/oss-fuzz) to better isolate workloads
    between different projects. OSS-Fuzz continues to improve the security of
    the overall web
    ([74](https://github.com/google/oss-fuzz/tree/master/projects) projects
    running 24x7,
    [636](https://bugs.chromium.org/p/oss-fuzz/issues/list?can=1&q=-component%3AInfra+status%3ANew%2CFixed%2CVerified+Type%3DBug-Security+&sort=-id&colspec=ID+Type+Component+Status+Proj+Reported+Owner+Summary&cells=ids)
    security bugs fixed)!

Enamel, Permissions

    We began [marking FTP as Not
    Secure](https://groups.google.com/a/chromium.org/d/msg/security-dev/HknIAQwMoWo/xYyezYV5AAAJ)
    with Chrome 63.

    We published a [paper](https://research.google.com/pubs/pub46359.html) in
    CCS 2017 describing years of work we’ve done to investigate and mitigate
    false-positive certificate errors. We also launched new improvements to help
    users who see lots of these spurious errors:

        We launched an interstitial to help users with buggy MITM Software with
        Chrome 63 (see chrome://interstitials/).

        We launched an interstitial to help users affected by Superfish with
        Chrome 61 (see chrome://interstitials/).

        Better integration with the OS for captive portal detection on Android
        and Windows in Chrome 63.

    Launched new [Site
    Details](https://docs.google.com/document/d/1gQG1-QjOuswdwZC-yMhWai7divOPXiCp0mO82SO5VSs/edit?pli=1)
    page.

    Removed non-factory-default settings from PageInfo and added back in the
    Certificate Viewer link.

    Launching [modal permission prompts on Android
    ](https://blog.chromium.org/2017/10/chrome-63-beta-dynamic-module-imports_27.html)in
    M63.

    Removed the ability to request Notification permission from iframes and over
    HTTP.

MOAR TLS

    The change to [mark HTTP
    pages](https://blog.chromium.org/2017/04/next-steps-toward-more-connection.html)
    in Incognito or after form field editing as Not Secure is in Chrome 62. We
    sent &gt; 1 million Search Console messages warning webmasters about this
    change.

    Google is [preloading HSTS for more
    TLDs](https://security.googleblog.com/2017/09/broadening-hsts-to-secure-more-of-web.html),
    the first new ones since .google was preloaded in 2015.

Chrome Safe Browsing

    Launched the [PVer4](https://developers.google.com/safe-browsing/v4/)
    database-update protocol to all users, saving them 80% of the bandwidth used
    by Safe Browsing.

Platform Security

    Added support for [new Win10 sandbox
    mitigations](https://bugs.chromium.org/p/chromium/issues/detail?id=733739)
    in M61 as part of our continued Windows Sandbox efforts.

    To help block 3rd-party code being injected into Chrome processes on Windows
    we've [Enabled third-party blocking on all child
    processes](https://bugs.chromium.org/p/chromium/issues/detail?id=750886#c1),
    after warmup (delayed mitigation), in M62.

    New in Android O, the Chrome-powered WebView component [now renders content
    in a separate, sandboxed
    process](https://android-developers.googleblog.com/2017/06/whats-new-in-webview-security.html)!
    This brings the same security and stability benefits of Chrome to web pages
    rendered within apps.

Site Isolation

    We launched [OOPIF](/developers/design-documents/oop-iframes)-based
    &lt;webview&gt; in M61 for ChromeOS/Mac/Linux and in M62 for Windows. This
    eliminates BrowserPlugin for everything except PDFs (which we're working on
    now), helping to clean up old code.

    Running an experiment in M63 to give process isolation to
    [accounts.google.com](http://accounts.google.com/), to improve Chrome Signin
    security.

    Finished design plans for using isolated processes when users click through
    SafeBrowsing malware warnings.

    Improved some of the Site Isolation enforcement mechanisms, including
    passwords and localStorage.

    Improved OOPIF support in several areas, including basic frame architecture
    (e.g., how proxy frames are created), touch selection editing, and gesture
    fling. Also making progress on OOPIF printing support.

As ever, many thanks to all those in the Chromium community who help make the
web more secure!

Cheers

Andrew, on behalf of the Chrome Security Team

## Q2 2017

Greetings and salutations,

It's time for another update from your friends in Chrome Security, who are hard
at work trying to make Chrome the[ most secure platform to browse the
Internet](/Home/chromium-security). Here’s a recap from last quarter:

The Bugs-- team have released a [new
tool](https://github.com/google/clusterfuzz-tools) to make ClusterFuzz testcase
reproduction easy for developers. Our open source fuzzing efforts (aka
[OSS-Fuzz](https://github.com/google/oss-fuzz)) continue to improve the security
of the overall web
([86](https://github.com/google/oss-fuzz/tree/master/projects) projects,
[1859](https://bugs.chromium.org/p/oss-fuzz/issues/list?can=1&q=-component:Infra%20status:New,Fixed,Verified&sort=-id&colspec=ID%20Type%20Component%20Status%20Proj%20Reported%20Owner%20Summary)
bugs, see recent blog post
[here](https://testing.googleblog.com/2017/05/oss-fuzz-five-months-later-and.html)).
We have written a new Javascript fuzzer that has filed
[102](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=inferno_js_fuzzer%20-status:Duplicate,WontFix%20OR%20inferno_js_fuzzer_c%20-status:Duplicate,WontFix&sort=-id+-security_severity+-secseverity+-owner+-modified&colspec=ID%20Pri%20Status%20Summary%20Modified%20OS%20M%20Security_severity%20Security_impact%20Owner%20Reporter)
bugs to date, many with security implications. We also found some interesting
vulnerabilities
([1](https://bugs.chromium.org/p/chromium/issues/detail?id=740710),
[2](https://bugs.chromium.org/p/chromium/issues/detail?id=716044),
[3](https://bugs.chromium.org/p/chromium/issues/detail?id=724299)) through our
code auditing efforts.

We integrated the Safe Browsing API with WebView [starting in Android
O](https://developer.android.com/preview/features/managing-webview.html),
allowing custom interstitial blocking pages. WebView developers will be able to
opt-in to check URLs against Google Safe Browsing’s list of unsafe websites.

We understand that sites which repeatedly prompt for powerful permissions often
annoy users and generate warning fatigue. Starting in Chrome 59, we’ve started
[temporarily blocking permission
requests](https://www.chromestatus.com/feature/6443143280984064) if users have
dismissed a permission prompt from a site multiple times. We’re also moving
forward with plans to [deprecate permissions in cross-origin
iframes](/Home/chromium-security/deprecating-permissions-in-cross-origin-iframes)
by default. Permission requests from iframes have the potential to mislead users
into granting access to content they didn’t intend.

The Platform Security team has concluded several years of A/B experimentation on
Android, and with Chrome 58 we have turned on the [Seccomp-BPF
sandbox](https://bugs.chromium.org/p/chromium/issues/detail?id=166704) for all
compatible devices. This sandbox filters system calls to reduce the attack
surface of the Linux kernel in renderer processes. Currently about 50% of
Android devices support Seccomp, and this number is rising at a steady rate. In
Chrome 59, you can navigate to about:sandbox to see whether your Android device
supports Seccomp.

We have migrated PDFium to use PartitionAlloc for most allocations, with
distinct partitions for strings, array buffers, and general allocations. In
Chrome 61, all three partitions will be active.

We continue to work on MOAR+BETTER TLS and
[announced](https://blog.chromium.org/2017/04/next-steps-toward-more-connection.html)
the next phase of our plan to help people understand the security limitations of
non-secure HTTP. Starting in Chrome 62 (October), we’ll mark HTTP pages as “Not
secure” when users enter data in forms, and on all HTTP pages in Incognito mode.
We [presented](https://youtu.be/GoXgl9r0Kjk) new HTTPS migration case studies at
Google I/O, focusing on real-world site metrics like SEO, ad revenue, and site
performance.

We experimented with improvements to Chrome’s captive portal detection on Canary
and launched them to stable in Chrome 59, to avoid a predicted 1% of all
certificate errors that users see.

Also, users [may
restore](https://textslashplain.com/2017/05/02/inspecting-certificates-in-chrome/)
the Certificate information to the Page Information bubble!

Those working on the Open Web Platform have implemented three new [Referrer
Policies](https://w3c.github.io/webappsec-referrer-policy/#referrer-policies),
giving developers more control over their HTTP Referer headers and bringing our
implementation in line with the spec. We also fixed a longstanding bug so that
site owners can now use [upgrade-insecure-requests in conjunction with CSP
reporting](https://w3c.github.io/webappsec-upgrade-insecure-requests/#reporting-upgrades),
allowing site owners to both upgrade and remediate HTTP references on their
HTTPS sites.

After our launch of --isolate-extensions in Chrome 56, the Site Isolation team
has been preparing for additional uses of [out-of-process
iframes](/developers/design-documents/oop-iframes) (OOPIFs). We implemented a
new --isolate-origins=https://example.com command line flag that can give
dedicated processes to a subset of origins, which is an important step towards
general [Site Isolation](/developers/design-documents/site-isolation). We also
prepared the OOPIF-based &lt;webview&gt; field trial for Beta and Stable
channels, and we ran a Canary field trial of Top Document Isolation to learn
about the performance impact of putting all cross-site iframes into one subframe
process. We've been improving general support for OOPIFs as well, including
spellcheck, screen orientation, touch selection, and printing. The DevTools team
has also helped out: OOPIFs can now be shown in the main frame's inspector
window, and DevTools extensions are now more fully isolated from DevTools
processes.

As ever, many thanks to all those in the Chromium community who help make the
web more secure!

## Q1 2017

Greetings and salutations,

It's time for another update from your friends in Chrome Security, who are hard
at work trying to make Chrome the[ most secure platform to browse the
Internet](/Home/chromium-security). Here’s a recap from last quarter:

Our [Bugs--](/Home/chromium-security/bugs) effort aims to find (and exterminate)
security bugs. In order to get bugs fixed faster, we released a [new
tool](https://github.com/google/clusterfuzz-tools) to improve developer
experience when trying to reproduce ClusterFuzz bugs. We have overhauled a
significant part of the [ClusterFuzz
UI](https://github.com/google/oss-fuzz/blob/master/docs/clusterfuzz.md) which
now feature a new fuzzer statistics page, crash statistics page and fuzzer
performance analyzer. We’ve also continued to improve our
[OSS-Fuzz](https://github.com/google/oss-fuzz)
[offering](https://opensource.googleblog.com/2016/12/announcing-oss-fuzz-continuous-fuzzing.html),
adding numerous
[features](https://github.com/google/oss-fuzz/issues?page=3&q=is%3Aissue+is%3Aclosed)
requested by developers and reaching [1000
bugs](https://bugs.chromium.org/p/oss-fuzz/issues/list?can=1&q=status%3AFixed%2CVerified+Type%3ABug%2CBug-Security+-component%3AInfra+)
milestone with 47
[projects](https://github.com/google/oss-fuzz/tree/master/projects) in just five
months since launch.

Members of the Chrome Security team attended the 10th annual [Pwn2Own
competition](http://blog.trendmicro.com/pwn2own-returns-for-2017-to-celebrate-10-years-of-exploits/)
at CanSecWest. While Chrome was again a target this year, no team was able to
demonstrate a fully working chain to Windows SYSTEM code execution in the time
allowed!

Bugs still happen, so our[ Guts](/Home/chromium-security/guts) effort builds in
multiple layers of defense. Chrome 56 takes advantage of [Control Flow Guard
(CFG)](https://msdn.microsoft.com/en-us/library/windows/desktop/mt637065(v=vs.85).aspx)
on Windows for Microsoft system DLLs inside the Chrome.exe processes. CFG makes
exploiting corruption vulnerabilities more challenging by limiting valid call
targets, and is available from Win 8.1 Update 3.

[Site Isolation](/developers/design-documents/site-isolation) makes the most of
Chrome's multi-process architecture to help reduce the scope of attacks. The big
news in Q1 is that we launched --isolate-extensions to Chrome Stable in Chrome
56! This first use of out-of-process iframes (OOPIFs) ensures that web content
is never put into an extension process. To maintain the launch and prepare for
additional uses of OOPIFs, we fixed numerous bugs, cleaned up old code, reduced
OOPIF memory usage, and added OOPIF support for more features (e.g.,
IntersectionObserver, and hit testing and IME on Android). Our next step is
expanding the OOPIF-based &lt;webview&gt; trial from Canary to Dev channel and
adding more uses of dedicated processes.

Beyond the browser, our [web platform efforts](/Home/chromium-security/owp)
foster cross-vendor cooperation on developer-facing security features. Over the
holidays, Google's security team gave us a holiday gift consisting entirely of
[interesting ways to bypass CSP's
nonces](http://sebastian-lekies.de/csp/bypasses.php). We've fixed some
[obvious](https://bugs.chromium.org/p/chromium/issues/detail?id=679291)
[bugs](https://bugs.chromium.org/p/chromium/issues/detail?id=680072) they
uncovered, and we'll continue working with other vendors to harden the spec and
our implementations. In other CSP news, we polished a mechanism to [enforce CSP
on child frames](https://w3c.github.io/webappsec-csp/embedded/), shipped a
[\`script-sample\`
property](https://groups.google.com/a/chromium.org/forum/#!msg/blink-dev/XlcpobBfJOI/8WYpiyk0CQAJ)
in CSP reports, and [allowed hashes to match external
scripts](https://groups.google.com/a/chromium.org/forum/#!msg/blink-dev/t2ai4lsHhWI/MndrZyEWCwAJ).
We're also gathering data to support a few [dangling markup
mitigations](https://groups.google.com/a/chromium.org/forum/#!msg/blink-dev/rOs6YRyBEpw/D3pzVwGJAgAJ),
and dropped support for subresource URLs with [embedded
credentials](https://groups.google.com/a/chromium.org/forum/#!msg/blink-dev/lx-U_JR2BF0/Hsg1fiZiBAAJ)
and [legacy
protocols](https://groups.google.com/a/chromium.org/forum/#!msg/blink-dev/bIJdwwoQ98U/-F1aL2FgBAAJ).

We also spend time building[ security
features](/developers/design-documents/safebrowsing) that[ users
see](/Home/chromium-security/enamel). To protect users from Data URI phishing
attacks, Chrome shows the “not secure” warning on Data URIs and [intends to
deprecate and
remove](https://groups.google.com/a/chromium.org/forum/#!topic/blink-dev/GbVcuwg_QjM)
content-initiated top-frame navigations to Data URIs. We also brought [AIA
fetching](https://docs.google.com/document/d/1ryqFMSHHRDERg1jm3LeVt7VMfxtXXrI8p49gmtniNP0/edit)
to Chrome for Android, and early metrics show over an 85% reduction in the
fraction of HTTPS warnings caused by misconfigured certificate chains on
Android. We made additional progress on improving Chrome’s captive portal
detection. Chrome now keeps precise attribution of where bad downloads come
from, so we can catch malware and UwS earlier. Chrome 57 also saw the launch of
a [secure time service](/developers/design-documents/sane-time), for which early
data shows detection of bad client clocks when validating certificates improving
from 78% to 95%.

We see migration to HTTPS as foundational to any web security whatsoever, so
we're actively working to drive #MOARTLS across Google and the Internet at
large. To help people understand the security limitations of non-secure HTTP,
Chrome now marks HTTP pages with passwords or credit card form fields as “not
secure” in the address bar, and is experimenting with in-form contextual
warnings. We’ll [remove
support](https://bugs.chromium.org/p/chromium/issues/detail?id=672605) for EME
over non-secure origins in Chrome 58, and we’ll [remove
support](https://groups.google.com/a/chromium.org/forum/m/#!topic/blink-dev/IVgkxkRNtMo)
for notifications over non-secure origins in Chrome 61. We talked about our
#MOARTLS methodology and the HTTPS business case at
[Enigma](https://www.youtube.com/watch?v=jplIY1GXBHM&feature=youtu.be).

In addition to #MOARTLS, we want to ensure more secure TLS through work on
protocols and the certificate ecosystem. TLS 1.3 is the next, major version of
the Transport Layer Security protocol. In Q1, Chrome tried the first,
significant deployment of TLS 1.3 by a browser. Based on what we learned from
that we hope to fully enable TLS 1.3 in Chrome in Q2.

In February, researchers from Google and CWI Amsterdam successfully mounted a
[collision](https://security.googleblog.com/2017/02/announcing-first-sha1-collision.html)
attack against the SHA-1 hash algorithm. It had been known to be weak for a very
long time, and in Chrome 56 dropped support for website certificates that used
SHA-1. This was the culmination of a plan first
[announced](https://security.googleblog.com/2014/09/gradually-sunsetting-sha-1.html)
back in 2014, which we've
[updated](https://security.googleblog.com/2015/12/an-update-on-sha-1-certificates-in.html)
a [few
times](https://security.googleblog.com/2016/11/sha-1-certificates-in-chrome.html)
since.

As ever, many thanks to all those in the Chromium community who help make the
web more secure!

Cheers

Andrew, on behalf of the Chrome Security Team

For more thrilling security updates and feisty rants,[ subscribe to
security-dev@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/security-dev).
You can find older updates at[
https://dev.chromium.org/Home/chromium-security/quarterly-updates](/Home/chromium-security/quarterly-updates).

## Q4 2016

Greetings and salutations,

It's time for another update from your friends in Chrome Security, who are hard
at work trying to make Chrome the[ most secure platform to browse the
Internet](/Home/chromium-security). Here’s a recap from the last quarter of
2016:

Our [Bugs--](/Home/chromium-security/bugs) effort aims to find (and exterminate)
security bugs.

We announced [OSS-Fuzz](https://github.com/google/oss-fuzz), a new Beta program
developed over the past years with the [Core Infrastructure
Initiative](https://www.coreinfrastructure.org/) community. This program will
provide continuous fuzzing for select core open source software. See full blog
post
[here](https://security.googleblog.com/2016/12/announcing-oss-fuzz-continuous-fuzzing.html).
So far, more than [50
projects](https://github.com/google/oss-fuzz/tree/master/projects) have been
integrated with OSS-Fuzz and we found ~350
[bugs](https://bugs.chromium.org/p/oss-fuzz/issues/list?can=1&q=Type%3ABug%2CBug-Security%20-component%3AInfra%20-status%3ADuplicate%2CWontFix&colspec=ID%20Type%20Component%20Status%20Proj%20Reported%20Owner%20Summary&num=100&start=300).

Security bugs submitted by external researchers can receive cash money from the
[Chrome VRP](https://g.co/ChromeBugRewards).

Last year the Chrome VRP paid out almost one million dollars! More details in a
[blog
post](https://security.googleblog.com/2017/01/vulnerability-rewards-program-2016-year.html)
we did with our colleagues in the Google and Android VRPs.

Bugs still happen, so our[ Guts](/Home/chromium-security/guts) effort builds in
multiple layers of defense.

[Win32k
lockdown](https://docs.google.com/document/d/1gJDlk-9xkh6_8M_awrczWCaUuyr0Zd2TKjNBCiPO_G4/edit#heading=h.ieb3qn2r8rq1)
for Pepper processes, including Adobe Flash and PDFium was shipped to Windows 10
clients on all channels in October 2016. Soon after the mitigation was enabled,
a Flash 0-day that used win32k.sys as a [privilege
escalation](https://technet.microsoft.com/en-us/library/security/ms16-135.aspx)
vector was discovered being used in the wild, and this was successfully blocked
by this mitigation! James Forshaw from Project Zero also wrote a
[blog](https://googleprojectzero.blogspot.com/2016/11/breaking-chain.html) about
the process of shipping this new mitigation.

A new security mitigation on &gt;= Win8 hit stable in October 2016 (Chrome 54).
This mitigation disables extension points (legacy hooking), blocking a number of
third-party injection vectors. Enabled on all child processes - [CL
chain](https://chromium.googlesource.com/chromium/src/+/c06d6fb1850d6217d35a5cccb1abccd6db0e7a2a).
As usual, you can find the Chromium sandbox documentation
[here](/developers/design-documents/sandbox).

[Site Isolation](/developers/design-documents/site-isolation) makes the most of
Chrome's multi-process architecture to help reduce the scope of attacks.

Our earlier plan to launch --isolate-extensions in Chrome 54 hit a last minute
delay, and we're now aiming to turn it on in Chrome 56. In the meantime, we've
added support for drag and drop into [out-of-process
iframes](/developers/design-documents/oop-iframes) (OOPIFs) and for printing an
OOPIF. We've fixed several other security and functional issues for
--isolate-extensions as well. We've also started an A/B trial on Canary to use
OOPIFs for Chrome App &lt;webview&gt; tags, and we're close to starting an A/B
trial of --top-document-isolation.

Beyond the browser, our[ web platform efforts](/Home/chromium-security/owp)
foster cross-vendor cooperation on developer-facing security features.

After a good deal of experimentation, we (finally) [tightened the behavior of
cookies' \`secure\`
attribute](https://www.chromestatus.com/feature/4506322921848832). [Referrer
Policy](https://www.w3.org/TR/referrer-policy/) moved to a candidate
recommendation, we made solid progress on
[Clear-Site-Data](https://w3c.github.io/webappsec-clear-site-data/), and we
expect to start an [origin
trial](https://github.com/jpchase/OriginTrials/blob/gh-pages/developer-guide.md)
for [Suborigins](https://w3c.github.io/webappsec-suborigins/) shortly.

Looking to the future, we've started to flesh out our proposal for [stronger
origin isolation properties](https://wicg.github.io/isolation/), continued
discussions on a proposal for [setting origin-wide
policy](https://wicg.github.io/origin-policy/), and began working with the IETF
to expand [opt-in Certificate Transparency
enforcement](https://datatracker.ietf.org/doc/draft-stark-expect-ct/) to the
open web. We hope to further solidify all of these proposals in Q1.

We also spend time building[ security
features](/developers/design-documents/safebrowsing) that[ users
see](/Home/chromium-security/enamel).

Our security indicator text labels launched in Chrome 55 for “Secure” HTTPS,
“Not Secure” broken HTTPS, and “Dangerous” pages flagged by Safe Browsing. As
part of our long-term effort to mark HTTP pages as non-secure, we built
[address-bar warnings into Chrome
56](https://blog.chromium.org/2016/12/chrome-56-beta-not-secure-warning-web.html)
to mark HTTP pages with a password or credit card form fields as “Not secure”.

We see migration to HTTPS as foundational to any web security whatsoever, so
we're actively working to drive #MOARTLS across Google and the Internet at
large.

We added a new [HTTPS Usage
section](https://www.google.com/transparencyreport/https/metrics/?hl=en) to the
Transparency Report, which shows how the percentage of Chrome pages loaded over
HTTPS increases with time. We talked externally at O’Reilly Security NYC +
Amsterdam and [Chrome Dev Summit](https://www.youtube.com/watch?v=iP75a1Y9saY)
about upcoming HTTP UI changes and the business case for HTTPS. We published
[positive
stories](https://blog.chromium.org/2016/11/heres-to-more-https-on-web.html)
about HTTPS migrations.

In addition to #MOARTLS, we want to ensure more secure TLS.

We [concluded](https://www.imperialviolet.org/2016/11/28/cecpq1.html) our
experiment with post-quantum key agreement in TLS. We implemented TLS 1.3 draft
18, which will be enabled for a fraction of users with Chrome 56.

And here are some other areas we're still investing heavily in:

Keeping users safe from Unwanted Software (UwS, pronounced 'ooze') and improving
the [Chrome Cleanup Tool](https://www.google.com/chrome/cleanup-tool/), which
has helped millions remove UwS that was injecting ads, changing settings, and
otherwise blighting their machines.

Working on usable, understandable permissions prompts. We're experimenting with
different prompt UIs, tracking prompt interaction rates, and continuing to learn
how best to ensure users are in control of powerful permissions.

As ever, many thanks to all those in the Chromium community who help make the
web more secure!

Cheers

Andrew, on behalf of the Chrome Security Team

For more thrilling security updates and feisty rants,[ subscribe to
security-dev@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/security-dev).
You can find older updates at[
https://dev.chromium.org/Home/chromium-security/quarterly-updates](/Home/chromium-security/quarterly-updates).

## Q3 2016

Greetings and salutations!

It's time for another update from your friends in Chrome Security, who are hard
at work trying to make Chrome the[ most secure platform to browse the
Internet](/Home/chromium-security). Here’s a recap from last quarter:

Our[ Bugs--](/Home/chromium-security/bugs) effort aims to find (and exterminate)
security bugs.

We have continued to improve upon our [libFuzzer](http://go/libfuzzer-chrome)
and
[AFL](https://cs.chromium.org/chromium/src/third_party/afl/?q=third_party/afl&sq=package:chromium&dr)
integration with ClusterFuzz, which includes automated performance analysis and
quarantining of bad units (like slow units, leaks, etc). We have scaled our code
coverage to
~[160](https://cs.chromium.org/search/?q=%22+int+LLVMFuzzerTestOneInput%22+-libFuzzer/src+-llvm/+-buildtools+-file:.md&sq=package:chromium&type=cs)
targets with help from Chrome developers, who contributed these during the
month-long
[Fuzzathon](https://groups.google.com/a/chromium.org/forum/#!topic/chromium-dev/MAiBRTllPuI).
We have improved our infrastructure reliability and response times by adding a
24x7 monitoring solution, and fixing more than two dozen fuzzers in the process.
Finally, we have refined our crash bucketization algorithm and enabled automatic
bug filing remove human latency in filing regression bugs — long live the
machines!

For [Site Isolation](/developers/design-documents/site-isolation), the first
uses of [out-of-process iframes](/developers/design-documents/oop-iframes)
(OOPIFs) have reached the Stable channel in Chrome 54!

We're using OOPIFs for --isolate-extensions mode, which ensures that web content
is never put into a privileged extension process. In the past quarter, we made
significant progress and fixed all our blocking bugs, including enabling the new
session history logic by default, supporting cross-process POST submissions, and
IME in OOPIFs. We also fixed bugs in painting, input events, and many other
areas. As a result, --isolate-extensions mode has been enabled for 50% of M54
Beta users and is turned on by default in M55. From here, we plan to further
improve OOPIFs to support --top-document-isolation mode, Chrome App
&lt;webview&gt; tags, and Site Isolation for real web sites.

We also spend time building[ security
features](/developers/design-documents/safebrowsing) that[ users
see](/Home/chromium-security/enamel).

We
[overhauled](https://docs.google.com/document/u/2/d/1jIfCjcsZUL6ouLgPOsMORGcTTXc7OTwkcBQCkZvDyGE/pub)
Chrome’s site security indicators in Chrome 52 on Mac and Chrome 53 on all other
platforms, including adding new icons for Safe Browsing. These icons were the
result of extensive user research which we shared in a [peer-reviewed
paper](https://www.usenix.org/system/files/conference/soups2016/soups2016-paper-porter-felt.pdf).
Lastly, we made recovering blocked-downloads much [less
confusing](https://docs.google.com/document/d/1M9AvvXafVRSNquKGhzjAqo3Pl7sSATuEECA78pfkpmA/edit#).

We like to avoid showing unnecessarily scary warnings when we can. We analyzed
data from opted-in Safe Browsing Extended Reporting users to quantify the major
causes of spurious TLS warnings, like [bad client
clocks](/developers/design-documents/sane-time) and [misconfigured intermediate
certificates](https://docs.google.com/document/d/1ryqFMSHHRDERg1jm3LeVt7VMfxtXXrI8p49gmtniNP0/edit?pli=1).
We also launched two experiments,
[Expect-CT](https://docs.google.com/document/d/1VDtHiKa5c96ohP_p-V1k6u83fIh952e_szZVypO4AvQ/edit)
and
[Expect-Staple](https://docs.google.com/document/d/1aISglJIIwglcOAhqNfK-2vtQl-_dWAapc-VLDh-9-BE/edit),
to help site owners deploy advanced new TLS features (Certificate Transparency
and OCSP stapling) without causing warnings for their users.

Beyond the browser, our[ web platform efforts](/Home/chromium-security/owp)
foster cross-vendor cooperation on developer-facing security features.

We continued to lock down the security of the web platform while also expanding
capabilities to developers. We helped lock down cookies by starting to ship
[Strict Secure Cookies](https://www.chromestatus.com/feature/4506322921848832).
Similarly, we also shipped the [Referrer
Policy](https://www.w3.org/TR/referrer-policy/) spec and policy header. [Content
Security Policy](https://www.w3.org/TR/CSP3/) was expanded with the
[strict-dynamic](https://www.w3.org/TR/CSP3/#strict-dynamic-usage) and
[unsafe-hashed-attributes](https://www.w3.org/TR/CSP3/#unsafe-hashed-attributes-usage)
directives. Our work on
[suborigins](https://w3c.github.io/webappsec-suborigins/) continued, updating
the serialization and adding new web platform support.

We've also been working on making users feel more in control of powerful
permissions.

In M55 and M56 we will be running experiments on permissions prompts to evaluate
how this affects acceptance and decision rates. The experiments are to let users
make temporary decisions, to auto-deny prompts if users keep ignoring them, and
making permission prompts modal.

We see migration to HTTPS as foundational to any web security whatsoever, so
we're actively working to drive #MOARTLS across Google and the Internet at
large.

We
[announced](https://security.googleblog.com/2016/09/moving-towards-more-secure-web.html)
concrete steps towards marking HTTP sites as non-secure in Chrome UI — starting
with marking HTTP pages with password or credit card form fields as “Not secure”
starting in Chrome 56 (Jan 2017). We [added YouTube and
Calendar](https://security.googleblog.com/2016/08/adding-youtube-and-calendar-to-https.html)
to the HTTPS Transparency Report. We’re also happy to report that
[www.google.com](http://www.google.com/) [uses
HSTS](https://security.googleblog.com/2016/07/bringing-hsts-to-wwwgooglecom.html)!

In addition to #MOARTLS, we want to ensure more secure TLS.

We continue to work on TLS 1.3, a major revision of TLS. For current revisions,
we’re also keeping the TLS ecosystem running smoothly with a little
[grease](https://tools.ietf.org/html/draft-davidben-tls-grease-01). We have
removed [DHE based
ciphers](https://www.chromestatus.com/feature/5128908798164992) and added
[RSA-PSS](https://www.chromestatus.com/features/5748550642171904). Finally,
having removed RC4 from Chrome earlier this year, we’ve now removed it from
BoringSSL’s TLS logic completely.

We launched a very rough prototype of
[Roughtime](https://roughtime.googlesource.com/roughtime), a combination of NTP
and Certificate Transparency. In parallel we’re investigating what reduction in
Chrome certificate errors a secure clock like Roughtime could give us.

We also continued our experiments with [post-quantum
cryptography](https://security.googleblog.com/2016/07/experimenting-with-post-quantum.html)
by implementing [CECPQ1](https://www.chromestatus.com/features/5749214348836864)
to help gather some real world data.

As ever, many thanks to all those in the Chromium community who help make the
web more secure!

Cheers

Andrew on behalf of the Chrome Security Team

## Q2 2016

Greetings Earthlings,

It's time for another update from your friends in Chrome Security, who are hard
at work trying to make Chrome the[ most secure platform to browse the
Internet](/Home/chromium-security). Here’s a recap from last quarter:

**Our[ Bugs--](/Home/chromium-security/bugs) effort aims to find (and
exterminate) security bugs.** At the start of the quarter, we initiated a
team-wide Security FixIt to trim the backlog of open issues… a bit of Spring
cleaning our issue tracker, if you will :) With the help of dozens of engineers
across Chrome, we fixed over 61 Medium+ severity security bugs in 2 weeks and
brought the count of open issues down to 22! On the fuzzing front, we’ve added
support for[ AFL](http://lcamtuf.coredump.cx/afl/) and continued to improve the[
libFuzzer](http://llvm.org/docs/LibFuzzer.html)-[ClusterFuzz](/Home/chromium-security/bugs/using-clusterfuzz)
integration, both of which allow coverage-guided testing on a per-function
basis. The number of libFuzzer based fuzzers have expanded from 70 to[
115](https://cs.chromium.org/search/?q=TestOneInput%5C(const+-file:third_party/llvm+-file:third_party/libFuzzer/src&sq=package:chromium&type=cs),
and we’re processing ~500 Billion testcases every day! We’re also researching
new ways to improve fuzzer efficiency and maximize code coverage
([example](https://chromium.googlesource.com/chromium/src/+/1a6bef1675e05626a4692ab6fa43cbbc5515299b)).
In response to recent trends from[ Vulnerability Reward Program
(VRP)](https://www.google.com/about/appsecurity/chrome-rewards/index.html) and[
Pwnium](http://blog.chromium.org/2015/02/pwnium-v-never-ending-pwnium.html)
submissions, we wrote a new fuzzer for v8 builtins, which has already yielded[
bugs](https://bugs.chromium.org/p/chromium/issues/detail?id=625752). Not
everything can be automated, so we started auditing parts of[
mojo](/developers/design-documents/mojo), Chrome’s new IPC mechanism, and found
several issues
([1](https://bugs.chromium.org/p/chromium/issues/detail?id=611887),[
2](https://bugs.chromium.org/p/chromium/issues/detail?id=612364),[
3](https://bugs.chromium.org/p/chromium/issues/detail?id=612613),[
4](https://bugs.chromium.org/p/chromium/issues/detail?id=613698),[
5](https://bugs.chromium.org/p/chromium/issues/detail?id=622522)).

**Bugs still happen, so our[ Guts](/Home/chromium-security/guts) effort builds
in multiple layers of defense.** Many Android apps use[
WebView](https://developer.android.com/reference/android/webkit/WebView.html) to
display web content inline within their app. A compromised WebView can get
access to an app’s private user data and a number of Android system services /
device drivers. To mitigate this risk, in the upcoming release of[ Android
N](https://developer.android.com/preview/support.html#dp3), we’ve worked to
move[
WebView](https://developer.android.com/reference/android/webkit/WebView.html)
rendering out-of-process into a sandboxed process. This new process model is
still experimental and can be enabled under Developer Options in Settings. On
Windows, a series of ongoing stability experiments with[ App
Container](/developers/design-documents/sandbox#TOC-App-Container-low-box-token-:)
and[ win32k
lockdown](/developers/design-documents/sandbox#TOC-Win32k.sys-lockdown:) for[
PPAPI](/nativeclient/getting-started/getting-started-background-and-basics#TOC-Pepper-Plugin-API-PPAPI-)
processes (i.e. Flash and pdfium) have given us good data that puts us in a
position to launch both of these new security mitigations on Windows 10 very
soon!

**For[ Site Isolation](/developers/design-documents/site-isolation), we're
getting close to enabling --isolate-extensions for everyone.** We've been hard
at work fixing launch blocking bugs, and[ out-of-process
iframes](/developers/design-documents/oop-iframes) (OOPIFs) now have support for
POST submissions, fullscreen, find-in-page, zoom, scrolling, Flash, modal
dialogs, and file choosers, among other features. We've also made lots of
progress on the new navigation codepath, IME, and the task manager, along with
fixing many layout tests and crashes. Finally, we're experimenting with
--top-document-isolation mode to keep the main page responsive despite slow
third party iframes, and with using OOPIFs to replace BrowserPlugin for the
&lt;webview&gt; tag.

**We also spend time building[ security
features](/developers/design-documents/safebrowsing) that[ users
see](/Home/chromium-security/enamel).** We’re overhauling the omnibox security
iconography in Chrome -- new,[
improved](https://www.usenix.org/conference/soups2016/technical-sessions/presentation/porter-felt)
connection security indicators are now in Chrome Beta (52) on Mac and Chrome Dev
(53) for all other platforms. We created a[
reference](https://github.com/google/safebrowsing) interstitial warning that
developers can use for their implementations of the[ Safe Browsing
API](https://developers.google.com/safe-browsing/). Speaking of Safe Browsing,
we’ve extended protection to cover files downloaded by Flash apps, we’re
evaluating many more file types than before, and we closed several gaps that
were reported via our Safe Browsing[ Download Protection
VRP](https://www.google.com/about/appsecurity/chrome-rewards/) program.

**Beyond the browser, our[ web platform efforts](/Home/chromium-security/owp)
foster cross-vendor cooperation on developer-facing security features.** We
shipped an implementation of the[ Credential Management
API](https://w3c.github.io/webappsec-credential-management/) (and[ presented a
detailed overview](https://youtu.be/MnvUlGFb3GQ?t=7m23s) at Google I/O),
iterated on[ Referrer Policy](https://www.w3.org/TR/referrer-policy/) with a[
\`referrer-policy\`
header](https://www.w3.org/TR/referrer-policy/#referrer-policy-header)
implementation behind a flag, and improved our support for[ SameSite
cookies](https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site). We're
continuing to experiment with[
Suborigins](https://w3c.github.io/webappsec-suborigins/) with developers both
inside and outside Google, built a prototype of[
CORS-RFC1918](https://mikewest.github.io/cors-rfc1918/), and introduce safety
nets to protect against XSS vulnerabilities due to browser bugs\[1\].

**We've also been working on making users feel more in control of powerful
permissions.** All permissions will soon be[ scoped to
origins](https://codereview.chromium.org/2075103002/), and we've started
implementing[ permission
delegation](https://noncombatant.github.io/permission-delegation-api/) (which is
becoming part of[ feature policy](https://github.com/igrigorik/feature-policy)).
We’re also actively working to show fewer permission prompts to users, and to
improve the prompts and UI we do show... subtle, critical work that make web
security more human-friendly (and thus, effective).

**We see migration to HTTPS as foundational to any web security whatsoever, so
we're actively working to drive #MOARTLS across Google and the Internet at
large.**[ Emily](https://twitter.com/estark37) and[
Emily](https://twitter.com/emschec) busted HTTPS myths for large audiences at[
Google
I/O](https://www.youtube.com/watch?v=YMfW1bfyGSY&index=19&list=PLOU2XLYxmsILe6_eGvDN3GyiodoV3qNSC)
and the[ Progressive Web App dev
summit](https://www.youtube.com/watch?v=e6DUrH56g14). The[ HSTS Preload
list](https://cs.chromium.org/chromium/src/net/http/transport_security_state_static.json)
has seen[ 3x growth](https://twitter.com/lgarron/status/747530273047273472)
since the beginning of the year – a great problem to have! We’ve addressed some[
growth
hurdles](https://docs.google.com/document/d/1LqpwT2aAekrWPtLui5GYdHSGlZNMNRYmPR14NXMRsQ4/edit#)
by a rewrite of the[ submission site](https://hstspreload.appspot.com/), and
we’re actively working on the preload list infrastructure and how to
additionally scale in the long term.

**In addition to #MOARTLS, we want to ensure more secure TLS.** Some of us have
been involved in the[ TLS 1.3 standardization
work](https://tlswg.github.io/tls13-spec/) and[
implementation](https://boringssl.googlesource.com/boringssl/). On the PKI
front, and as part of our[ Expect CT
project](https://docs.google.com/document/d/1VDtHiKa5c96ohP_p-V1k6u83fIh952e_szZVypO4AvQ/edit),
we built the infrastructure in Chrome that will help site owners track down
certificates for their sites that are not publicly logged in[ Certificate
Transparency](https://www.certificate-transparency.org/) logs. As of Chrome 53,
we’ll be requiring Certificate Transparency information for certificates issued
by Symantec-operated CAs, per[ our announcement last
year](https://security.googleblog.com/2015/10/sustaining-digital-certificate-security.html).
We also[ launched some post-quantum cipher suite
experiments](https://security.googleblog.com/2016/07/experimenting-with-post-quantum.html)
to protect everyone from... crypto hackers of the future and more advanced
worlds ;)

For more thrilling security updates and feisty rants, [subscribe to
security-dev@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/security-dev).
You can find older updates at
<https://dev.chromium.org/Home/chromium-security/quarterly-updates>.

Happy Hacking,

Parisa, on behalf of Chrome Security

\[1\] Please[ let us know](/Home/chromium-security/reporting-security-bugs) if
you manage to work around them!

## Q1 2016

Greetings web fans,

The[ Bugs--](/Home/chromium-security/bugs) effort aims to find (and exterminate)
security bugs. On the fuzzing front, we’ve continued to improve the integration
between [libFuzzer](http://llvm.org/docs/LibFuzzer.html) and
[ClusterFuzz](/Home/chromium-security/bugs/using-clusterfuzz), which allows
coverage-guided testing on a per-function basis. With the help of many
developers across several teams, we’ve expanded our collection of fuzzing
targets in Chromium (that use libFuzzer) to 70! Not all bugs can be found by
fuzzing, so we invest effort in targeted code audits too. We wrote a [guest post
on the Project Zero
blog](https://googleprojectzero.blogspot.com/2016/02/racing-midi-messages-in-chrome.html)
describing one of the more interesting vulnerabilities we discovered. Since we
find a lot of bugs, we also want to make them easier to manage. We’ve updated
our [Sheriffbot](/issue-tracking/autotriage) tool to simplify the addition of
new rules and expanded it to help manage functional bugs in addition just
security issues. We’ve also automated assigning [security
severity](/developers/severity-guidelines) recommendations. Finally, we continue
to run our [vulnerability reward
program](https://www.google.com/about/appsecurity/chrome-rewards/) to recognize
bugs discovered from researchers outside of the team. As of M50, [we’ve paid out
over $2.5
million](https://chrome.googleblog.com/2016/04/chrome-50-releases-and-counting.html)
since the start of the reward program, including over $500,000 in 2015. Our
median payment amount for 2015 was $3,000 (up from $2,000 for 2014), and we want
to see that increase again this year!

Bugs still happen, so our[ Guts](/Home/chromium-security/guts) effort builds in
multiple layers of defense. On Android, our
[seccomp-bpf](https://www.kernel.org/doc/Documentation/prctl/seccomp_filter.txt)
experiment has been running on the Dev channel and will advance to the Stable
and Beta channels with M50.

Chrome on Windows is evolving rapidly in step with the operating system. We
shipped [four new layers of defense in
depth](https://bugs.chromium.org/p/chromium/issues/detail?id=504006) to take
advantage of the latest capabilities in Windows 10, some of which patch
vulnerabilities found by our own
[research](https://googleprojectzero.blogspot.co.uk/2015/05/in-console-able.html)
and
[feedback](https://bugs.chromium.org/p/project-zero/issues/detail?id=213&redir=1)!
There was great media attention when these changes landed, from [Ars
Technica](http://arstechnica.com/information-technology/2016/02/chrome-picks-up-bonus-security-features-on-windows-10/)
to a [Risky Business podcast](http://risky.biz/RB398), which said: “There have
been some engineering changes to Chrome on Windows 10 which look pretty good. …
It’s definitely the go-to browser, when it comes to not getting owned on the
internet. And it’s a great example of Google pushing the state of the art in
operating systems.”

For our[ Site Isolation](/developers/design-documents/site-isolation) effort, we
have expanded our on-going launch trial of --isolate-extensions to include 50%
of both Dev Channel and Canary Channel users! This mode uses [out-of-process
iframes](/developers/design-documents/oop-iframes) (OOPIFs) to keep dangerous
web content out of extension processes. (See
[here](/developers/design-documents/oop-iframes#TOC-Dogfooding) for how to try
it.) We've fixed many launch blocking bugs, and improved support for navigation,
input events, hit testing, and security features like CSP and mixed content. We
improved our test coverage and made progress on updating features like
fullscreen, zoom, and find-in-page to work with OOPIFs. We're also excited to
see progress on other potential uses of OOPIFs, including the &lt;webview&gt;
tag and an experimental "top document isolation" mode.

We spend time building[ security
features](/developers/design-documents/safebrowsing) that[ people
see](/Home/chromium-security/enamel). In response to user feedback, we’ve
replaced [the old full screen
prompt](https://bugs.chromium.org/p/chromium/issues/attachment?aid=178299&inline=1)
with a [new, lighter weight ephemeral
message](https://bugs.chromium.org/p/chromium/issues/attachment?aid=222712&inline=1)
in M50 across Windows and Linux. We launched a few bug fixes and updates to the
[Security
panel](https://developers.google.com/web/updates/2015/12/security-panel?hl=en),
which we continue to iterate on and support in an effort to drive forward HTTPS
adoption. We also continued our work on [removing powerful features on insecure
origins](/Home/chromium-security/deprecating-powerful-features-on-insecure-origins)
(e.g. [geolocation](https://crbug.com/561641)).

We’re working on preventing abuse of powerful features on the web. We continue
to support great “permissions request” UX, and have started reaching out to top
websites to directly help them improve how they request permissions for
[powerful
APIs](/Home/chromium-security/prefer-secure-origins-for-powerful-new-features).
To give top-level websites more control over how iframes use permissions, we
started external discussions about a new [Permission Delegation
API](http://noncombatant.github.io/permission-delegation-api/). We also
[extended](https://security.googleblog.com/2016/03/get-rich-or-hack-tryin.html)
our [vulnerability rewards
program](https://www.google.com/about/appsecurity/chrome-rewards/index.html#rewards)
to support [Safe
Browsing](https://www.google.com/transparencyreport/safebrowsing/) reports, in a
first program of its kind.

Beyond the browser, our[ web platform efforts](/Home/chromium-security/owp)
foster cross-vendor cooperation on developer-facing security features. We now
have an implementation of
[Suborigins](https://w3c.github.io/webappsec-suborigins/) behind a flag, and
have been experimenting with Google developers on usage. We polished up the
[Referrer Policy](https://www.w3.org/TR/referrer-policy/) spec, refined its
integration with [ServiceWorker](https://www.w3.org/TR/service-workers/) and
Fetch, and shipped the \`referrerpolicy\` attribute from that document. We're
excited about the potential of new CSP expressions like
['unsafe-dynamic'](https://w3c.github.io/webappsec-csp/#unsafe-dynamic-usage),
which will ship in Chrome 52 (and is experimentally deployed on [our shiny new
bug tracker](https://bugs.chromium.org/p/chromium/issues/list)). In that same
release, we finally shipped [SameSite
cookies](https://tools.ietf.org/html/draft-west-first-party-cookies), which we
hope will help prevent
[CSRF](https://en.wikipedia.org/wiki/Cross-site_request_forgery). Lastly, we're
working to pay down some technical debt by refactoring our Mixed Content
implementation and X-Frame-Options to work in an OOPIF world.

We see migration to HTTPS as foundational to any security whatsoever
([and](https://tools.ietf.org/html/rfc7258)[
we're](https://www.iab.org/2014/11/14/iab-statement-on-internet-confidentiality/)[
not](https://www.iab.net/iablog/2015/03/adopting-encryption-the-need-for-https.html)[
the](https://w3ctag.github.io/web-https/)[ only](https://https.cio.gov/)[
ones](https://blog.mozilla.org/security/2015/04/30/deprecating-non-secure-http/)),
so we're actively working to drive #MOARTLS across Google and the Internet at
large. We worked with a number of teams across Google to help [publish an HTTPS
Report
Card](https://security.googleblog.com/2016/03/securing-web-together_15.html),
which aims to hold Google and other top sites accountable, as well as encourage
others to encrypt the web. In addition to #MOARTLS, we want to ensure more
secure TLS. [We mentioned we were working on it last
time](https://groups.google.com/a/chromium.org/forum/#!msg/security-dev/kVfCywocUO8/vgi_rQuhKgAJ),
but RC4 support is dead! The [insecure TLS version fallback is also
gone](https://groups.google.com/a/chromium.org/forum/#!msg/blink-dev/yz1lU9YTeys/yCsK50I3CQAJ).
With help from the [libFuzzer](http://llvm.org/docs/LibFuzzer.html) folks, we
got much better fuzzing coverage on
[BoringSSL](https://boringssl.googlesource.com/boringssl/), which resulted in
[CVE-2016-0705](https://web.nvd.nist.gov/view/vuln/detail?vulnId=CVE-2016-0705).
We ended up adding a "fuzzer mode" to the SSL stack to help the fuzzer get past
cryptographic invariants in the handshake, which smoked out some minor (memory
leak) bugs.

Last, but not least, we rewrote a large chunk of BoringSSL's ASN.1 parsing with
a simpler and more standards-compliant stack.

For more thrilling security updates and feisty rants, subscribe to
[security-dev@chromium.org](mailto:security-dev@chromium.org). You can find
older updates at
<https://dev.chromium.org/Home/chromium-security/quarterly-updates>.

Happy Hacking,

Parisa, on behalf of Chrome Security

## Q4 2015

Happy 2016 from the Chrome Security Team!

For those that don’t know us already, we do stuff to help make Chrome the [most
secure platform to browse the Internet](/Home/chromium-security). Here’s a recap
of some work from last quarter:

**The [Bugs--](/Home/chromium-security/bugs) effort aims to find (and
exterminate) security bugs.** We’ve integrated
[libFuzzer](http://llvm.org/docs/LibFuzzer.html) into
[ClusterFuzz](/Home/chromium-security/bugs/using-clusterfuzz), which means we
can do coverage-guided fuzz testing on a per-function basis. The result, as you
may have guessed, is several [new
bugs](https://code.google.com/p/chromium/issues/list?can=1&q=type%3Abug-security+label%3AStability-LibFuzzer&sort=-security_severity+-secseverity+-owner+-modified&colspec=ID+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner+Reporter&x=m&y=releaseblock&cells=tiles).
The Bugs-- team has a larger goal this year to help Chromium developers write a
ClusterFuzz fuzzer alongside every unittest, and libFuzzer integration is an
important step toward achieving that goal. Separately, we’ve made security
improvements and cleanups in the [Pdfium](https://code.google.com/p/pdfium/)
codebase and [fixed lots of open
bugs](https://code.google.com/p/chromium/issues/list?can=1&q=status%3AFixed+type%3DBug-Security+owner%3Aochang+Cr%3DInternals-Plugins-PDF).
We also started some manual code auditing efforts, and discovered several high
severity bugs ([here](https://crbug.com/555784),
[here](https://crbug.com/559310), and [here](https://crbug.com/570427)), and
[1](https://crbug.com/564501) critical severity bug.

**Bugs still happen, so our [Guts](/Home/chromium-security/guts) effort builds
in multiple layers of defense.** On Android, we’re running an experiment that
adds an additional
[seccomp-bpf](https://www.kernel.org/doc/Documentation/prctl/seccomp_filter.txt)
sandbox to renderer processes, like we already do on Desktop Linux and Chrome
OS. On Windows 8 (and above), a Win32k lockdown experiment has been implemented
for PPAPI plugins including Flash and Pdfium to help reduce the kernel attack
surface for potential sandbox escapes. Also on Windows 8 (and above), an
AppContainer sandbox experiment has been introduced, which further reduces
kernel attack surface and blocks network communication from renderers.

**Our [Site Isolation](/developers/design-documents/site-isolation) effort
reached a large milestone in December: running trials of the
--isolate-extensions mode on real Chrome Canary users!** This mode uses
[out-of-process iframes](/developers/design-documents/oop-iframes) to isolate
extension processes from web content for security. ([Give it a
try!](/developers/design-documents/oop-iframes#TOC-Dogfooding)) The trials were
made possible by many updates to session history, session restore, extensions,
painting, focus, save page, popup menus, and more, as well as numerous crash
fixes. We are continuing to fix the [remaining blocking
issues](https://csreis.github.io/oop-iframe-dependencies/), and we aim to launch
both --isolate-extensions and the broader Site Isolation feature in 2016.

**We also spend time building [security
features](/developers/design-documents/safebrowsing) that [users
see](/Home/chromium-security/enamel).** The Safe Browsing team publicly
announced a [new social engineering
policy](https://googleonlinesecurity.blogspot.com/2015/11/safe-browsing-protection-from-even-more.html),
expanding Chrome’s protection against deceptive sites beyond phishing. One major
milestone is the [launch of Safe Browsing in Chrome for
Android](https://googleonlinesecurity.blogspot.com/2015/12/protecting-hundreds-of-millions-more.html),
protecting hundreds of millions of additional users from phishing, malware, and
other web threats! This is on by default and is already stopping millions of
attacks on mobile Chrome users. The next time you come across a Safe Browsing
warning, you can search for the blocked website in the
[new](https://googleonlinesecurity.blogspot.com/2015/10/behind-red-warning-more-info-about.html)
[Site Status
section](https://www.google.com/transparencyreport/safebrowsing/diagnostic/) of
the Transparency Report to learn why it’s been flagged by our systems. On the
other hand, we’re also trying to show users fewer security warnings in the first
place by decreasing our false positive rate for HTTPS warnings. We spent a large
part of the quarter analyzing client errors that contribute to false alarm HTTPS
errors; check out our [Real World Crypto
talk](https://docs.google.com/presentation/d/1Qmpl-5epx0B5C2t4XsUTyjgbwab_rXfK_4iHqX3IC30/edit?pref=2&pli=1#slide=id.gf44795496_0_1)
for more details.

**Beyond the browser, our [web platform efforts](/Home/chromium-security/owp)
foster cross-vendor cooperation on developer-facing security features.** We've
made good progress [with folks in the
IETF](https://lists.w3.org/Archives/Public/ietf-http-wg/2015OctDec/0165.html) to
make some meaningful changes to cookies; [cookie
prefixes](https://www.chromestatus.com/features/4952188392570880) and [locking
down 'secure' cookies](https://www.chromestatus.com/features/4506322921848832)
will be shipping shortly. [Subresource
Integrity](https://lists.w3.org/Archives/Public/public-webappsec/2016Jan/0020.html)
and [Mixed
Content](https://lists.w3.org/Archives/Public/public-webappsec/2016Jan/0021.html)
are trucking along the W3C Recommendation path, we've solidified our [Suborigins
proposal](https://w3c.github.io/webappsec-suborigins/), and have our eyes on
some new hotness like [HSTS Priming](https://mikewest.github.io/hsts-priming/),
[CSP3](https://w3c.github.io/webappsec-csp/)
[bits](https://w3c.github.io/webappsec-csp/embedded/)
[and](https://w3c.github.io/reporting/)
[pieces](https://w3c.github.io/webappsec-csp/cookies/), and [limiting access to
local network resources](https://mikewest.github.io/cors-rfc1918/).

**We see migration to HTTPS as foundational to any security whatsoever
([and](https://tools.ietf.org/html/rfc7258)
[we're](https://www.iab.org/2014/11/14/iab-statement-on-internet-confidentiality/)
[not](https://www.iab.net/iablog/2015/03/adopting-encryption-the-need-for-https.html)
[the](https://w3ctag.github.io/web-https/) [only](https://https.cio.gov/)
[ones](https://blog.mozilla.org/security/2015/04/30/deprecating-non-secure-http/)),
so we're actively working to drive #MOARTLS across Google and the Internet at
large.** We've continued our effort to [deprecate powerful features on insecure
origins](/Home/chromium-security/deprecating-powerful-features-on-insecure-origins)
by readying to block insecure usage of [geolocation
APIs](http://dev.w3.org/geo/api/spec-source.html). We also took to the
[stage](https://www.youtube.com/watch?v=9WuP4KcDBpI) at the [Chrome Dev
Summit](https://developer.chrome.com/devsummit) to spread the word, telling
developers about what we’re doing in Chrome to make deploying TLS easier and
more secure.

In addition to more TLS, we want to ensure more secure TLS, which depends
heavily on the certificate ecosystem. Via [Certificate
Transparency](http://www.certificate-transparency.org/), we [detected a
fraudulent Symantec-issued certificate in
September](https://googleonlinesecurity.blogspot.com/2015/09/improved-digital-certificate-security.html),
which [subsequently revealed a pattern of additional misissued
certificates](https://googleonlinesecurity.blogspot.com/2015/10/sustaining-digital-certificate-security.html).
Independent of that incident, we took proactive measures to protect users from a
Symantec Root Certificate that was being decommissioned in a way that puts users
at risk (i.e. no longer complying with the CA/Browser Forum’s [Baseline
Requirements](https://cabforum.org/about-the-baseline-requirements/)). Other
efforts include working with
[Mozilla](https://groups.google.com/forum/#!topic/mozilla.dev.platform/JIEFcrGhqSM/discussion)
and
[Microsoft](https://blogs.windows.com/msedgedev/2015/09/01/ending-support-for-the-rc4-cipher-in-microsoft-edge-and-internet-explorer-11/)
to [phase out RC4 ciphersuite
support](https://groups.google.com/a/chromium.org/forum/#!msg/security-dev/kVfCywocUO8/vgi_rQuhKgAJ),
and [continuing the deprecation of SHA-1
certificates](https://googleonlinesecurity.blogspot.com/2015/12/an-update-on-sha-1-certificates-in.html),
which were shown to be [even weaker than previously
believed](https://sites.google.com/site/itstheshappening/). To make it easier
for developers and site operators to understand these changes, we debuted a new
[Security
Panel](https://developers.google.com/web/updates/2015/12/security-panel?hl=en)
that provides enhanced diagnostics and will continue to be improved with richer
diagnostics in the coming months.

For more thrilling security updates and feisty rants, subscribe to
[security-dev@chromium.org](mailto:security-dev@chromium.org). You can find
older updates at
<https://dev.chromium.org/Home/chromium-security/quarterly-updates>.

Happy Hacking,

Parisa, on behalf of Chrome Security

## Q3 2015

Hello from the Chrome Security Team!

For those that don’t know us already, we do stuff to help make Chrome the[ most
secure platform to browse the Internet](/Home/chromium-security). Here’s a recap
of some work from last quarter:

The[ Bugs--](/Home/chromium-security/bugs) effort aims to find (and exterminate)
security bugs. We’ve continued our collaboration with Android Security team and
now have a fully functional
[AddressSanitizer](http://clang.llvm.org/docs/AddressSanitizer.html) (ASAN)
build configuration of [AOSP](https://source.android.com/index.html) master
(public instructions
[here](http://source.android.com/devices/tech/debug/asan.html)).
[ClusterFuzz](/Home/chromium-security/bugs/using-clusterfuzz) is helping Android
Security team triage and verify bugs, including incoming [vulnerability
reward](https://www.google.com/about/appsecurity/android-rewards/) submissions,
and now supports custom APK uploads and the ability to launch commands. Back on
the Chrome front, we’re working on enabling [Control Flow Integrity (CFI)
checks](https://code.google.com/p/chromium/issues/detail?id=464797) on Linux,
which converts invalid vptr accesses into non-exploitable crashes; [8
bugs](https://code.google.com/p/chromium/issues/list?can=1&q=linux_cfi_chrome&colspec=ID+Pri+M+Stars+ReleaseBlock+Cr+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=tiles)
discovered so far! We’ve made [numerous
improvements](https://code.google.com/p/chromium/issues/detail?id=511270) to how
we fuzz Chrome on Android with respect to speed and accuracy. We also made some
progress toward our goal of expanding ClusterFuzz platform support to include
iOS. In our efforts to improve Chrome Stability, we added
[LeakSanitizer](http://clang.llvm.org/docs/LeakSanitizer.html) (LSAN) into our
list of supported memory tools, which has already found [38
bugs](https://code.google.com/p/chromium/issues/list?can=1&q=label%3AClusterFuzz+-status%3AWontFix%2CDuplicate+%22-leak%22+linux_lsan_chrome_mp&sort=id+-security_severity+-secseverity+-owner+-modified&colspec=ID+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner+Reporter&x=m&y=releaseblock&cells=tiles).

Bugs still happen, so our[ Guts](/Home/chromium-security/guts) effort builds in
multiple layers of defense. Plugin security remains a very important area of
work. With the final [death](https://g.co/npapi) of unsandboxed NPAPI plugins in
September, we’ve continued to introduce mitigations for the remaining sandboxed
PPAPI (Pepper) plugins. First, we implemented [support for Flash component
updates on Linux](https://codereview.chromium.org/1261333004/), a long-standing
feature request, which allows us to respond to [Flash
0-day](http://www.zdnet.com/article/adobe-releases-emergency-patch-for-flash-zero-day-flaw/)
incidents without waiting to qualify a new release of Chrome. We’ve also been
spending time improving the code quality and test coverage of
[Pdfium](https://code.google.com/p/pdfium/), the [now open-source version of the
Foxit PDF
reader](https://plus.sandbox.google.com/+FrancoisBeaufort/posts/9wwSiWDDKKP). In
addition, we have been having some success with enabling [Win32k syscall
filtering](/developers/design-documents/sandbox#TOC-Process-mitigation-policies-Win32k-lockdown-)
on Windows PPAPI processes (PDFium and Adobe Flash). This makes it even tougher
for attackers to get out of the Chromium Flash sandbox, and can be enabled on
Windows 8 and above on Canary channel right now by toggling the settings in
chrome://flags/#enable-ppapi-win32k-lockdown.

We’ve been making steady progress on [Site
Isolation](/developers/design-documents/site-isolation), and are preparing to
enable out-of-process iframes (OOPIFs) for web pages inside extension processes.
You can test this mode before it launches with --isolate-extensions. We have
[performance
bots](https://chromeperf.appspot.com/report?sid=74ebe5d13c09e34915152f51ee178f77421b1703d2bf26a7a586375f2c5e2cc7)
and UMA stats lined up, and we'll start with some early trials on Canary and Dev
channel. Meanwhile, we've added support for hit testing in the browser process,
scrolling, context menus, and script calls between all reachable frames (even
with changes to window.opener).

Not all security problems can be solved in[ Chrome’s
guts](/Home/chromium-security/guts), so[ we work on making security more
user-friendly](/Home/chromium-security/enamel) too. To support developers
migrating to HTTPS, starting with M46, Chrome is marking the “[HTTPS with Minor
Errors](https://googleonlinesecurity.blogspot.com/2015/10/simplifying-page-security-icon-in-chrome.html)”
state using the same neutral page icon as HTTP pages (instead of showing the
yellow lock icon). We’ve started analyzing invalid (anonymized!) TLS certificate
reports gathered from the field, to understand the root causes of unnecessary
TLS/SSL warnings. One of the first causes we identified and fixed was
certificate hostname mismatches due to a missing ‘www’. We also launched
[HPKP](https://tools.ietf.org/html/rfc7469) violation reporting in Chrome,
helping developers detect misconfigurations and attacks by sending a report when
a pin is violated. Finally, in an effort to support the Chrome experience across
languages and locales, we made strides in improving how the omnibox is displayed
in [RTL languages](https://en.wikipedia.org/wiki/Right-to-left).

Beyond the browser, our [web platform efforts](/Home/chromium-security/owp)
foster cross-vendor cooperation on developer-facing security features. We
shipped [Subresource
Integrity](https://w3c.github.io/webappsec-subresource-integrity/) (SRI), which
defends against resource substitution attacks by allowing developers to specify
a hash against which a script or stylesheet is matched before it's executed.
We’re excited to see large sites, like
[Github](http://githubengineering.com/subresource-integrity/), already deploying
SRI! We've sketched out a concept for a [Clear Site
Data](http://w3c.github.io/webappsec-clear-site-data/) feature which we hope
will make it possible for sites to reset their storage, and we're hard at work
on the next iteration of [Content Security
Policy](https://w3c.github.io/webappsec-csp/). Both of these will hopefully
start seeing some implementation in Q4.

We see migration to HTTPS as foundational to any security whatsoever
([and](https://tools.ietf.org/html/rfc7258)
[we're](https://www.iab.org/2014/11/14/iab-statement-on-internet-confidentiality/)
[not](https://www.iab.net/iablog/2015/03/adopting-encryption-the-need-for-https.html)
[the](https://w3ctag.github.io/web-https/) [only](https://https.cio.gov/)
[ones](https://blog.mozilla.org/security/2015/04/30/deprecating-non-secure-http/)),
so we're actively working to drive #MOARTLS across Google and the Internet at
large. We shipped [Upgrade Insecure
Requests](http://www.w3.org/TR/upgrade-insecure-requests/), which eases the
transition to HTTPS by transparently correcting a page's spelling from
\`http://\` to \`https://\` for all resources before any requests are triggered.
We've also continued our effort to [deprecate powerful features on insecure
origins](/Home/chromium-security/deprecating-powerful-features-on-insecure-origins)
by solidifying the definition of a "[Secure
Context](https://w3c.github.io/webappsec-secure-contexts/)", and applying that
definition to block insecure usage of
[getUserMedia()](https://groups.google.com/a/chromium.org/d/msg/blink-dev/Dsoq5xPdzyw/21znuLWVCgAJ).

For more thrilling security updates and feisty rants, subscribe to
[security-dev@chromium.org](mailto:security-dev@chromium.org).

Happy Hacking,

Parisa, on behalf of Chrome Security

## Q2 2015

Hello from the Chrome Security Team!

For those that don’t know us already, we do stuff to help make Chrome the[ most
secure platform to browse the Internet](/Home/chromium-security). Here’s a recap
of some work from last quarter:

The[ Bugs--](/Home/chromium-security/bugs) effort aims to find (and exterminate)
security bugs. At the start of the quarter, we initiated a [Security
FixIt](https://docs.google.com/spreadsheets/d/1za0aWxDx4ga3dKBwrlS8COci6_VXLBaiVg3x5Y6rrFg/edit#gid=0)
to trim back the fat backlog of open issues. With the help of dozens of
engineers across Chrome, we fixed over 40 Medium+ severity security bugs in 2
weeks and brought the count of issues down to 15! We also collaborated with
Android Security Attacks Team and added native platform fuzzing support to
[ClusterFuzz](/Home/chromium-security/bugs/using-clusterfuzz) (and
[imported](https://cs.corp.google.com/#android/vendor/google_experimental/users/natashenka/&q=natashenka)
[their](https://cs.corp.google.com/#android/vendor/google/tools/security/ServiceProbe/src/com/google/wireless/android/security/serviceprobe/ServiceFuzzer.java&l=22)
[fuzzers](https://cs.corp.google.com/#android/vendor/google_experimental/users/jlarimer/lhf/README.TXT&l=1)),
which resulted in ~30 new bugs discovered. ClusterFuzz now supports fuzzing on
all devices of the Nexus family (5,6,7,9) and Android One and is running on a
few dozen devices in the Android Lab. On top of this, we have doubled our
fuzzing capacity in [Compute Engine](https://cloud.google.com/compute/) to ~8000
cores by leveraging [Preemptible
VMs](http://googlecloudplatform.blogspot.com/2015/05/Introducing-Preemptible-VMs-a-new-class-of-compute-available-at-70-off-standard-pricing.html).
Lastly, we have upgraded all of our sanitizer builds on Linux (ASan, MSan, TSan
and UBSan) to report [edge-level
coverage](http://clang.llvm.org/docs/SanitizerCoverage.html) data, which is now
aggregated in the ClusterFuzz
[dashboard](https://cluster-fuzz.appspot.com/#coverage). We’re using this
coverage information to expand data bundles by existing fuzzers and improve our
corpus distillation.

Bugs still happen, so our[ Guts](/Home/chromium-security/guts) effort builds in
multiple layers of defense. Our [Site
Isolation](/developers/design-documents/site-isolation) project is getting
closer to its first stage of launch: using out-of-process iframes (OOPIFs) for
web pages inside extension processes. We've made substantial progress (with lots
of help from others on the Chrome team!) on core Chrome features when using
--site-per-process: OOPIFs now work with back/forward, DevTools, and extensions,
and they use [Surfaces](/developers/design-documents/chromium-graphics/surfaces)
for efficient painting (and soon input event hit-testing). We've collected some
preliminary performance data using [Telemetry](/developers/telemetry), we've
fixed lots of crashes, and we've started enforcing cross-site security
restrictions on cookies and passwords. Much work remains, but we're looking
forward to turning on these protections for real users!

On Linux and Chrome OS, we’ve made changes to restrict [one PID namespace per
renderer process](https://crbug.com/460972), which strengthens and cleans-up our
sandbox (shipping in Chrome 45). We also finished up a [major cleanup necessary
toward deprecating the setuid sandbox](https://crbug.com/312380), which should
be happening soon. Work continued to prepare for the launch of Windows 10, which
offers some opportunities for new security
[mitigations](/developers/design-documents/sandbox#TOC-Process-mitigation-policies-Win32k-lockdown-);
the new version looks like the most secure Windows yet, so be sure to upgrade
when it comes out!

Not all security problems can be solved in[ Chrome’s
guts](/Home/chromium-security/guts), so[ we work on making security more
user-friendly](/Home/chromium-security/enamel) too. We’ve continued our efforts
to avoid showing unnecessary TLS/SSL warnings: decisions are now remembered for
a week instead of a session, and a [new checkbox on TLS/SSL warnings allows
users to send us invalid certificate chains
](https://www.google.com/chrome/browser/privacy/whitepaper.html#ssl)that help us
root out false-positive warnings. Since developers and power users have been
asking for more tools to debug TLS/SSL issues, we’ve started building more
security information into DevTools and plan to launch a first version in Q3!

Another large focus for the team has been improving how users are asked for
permissions, like camera and geolocation. We’ve finalized a redesign of the
fullscreen permission flow that we hope to launch by the end of the year, fixed
a number of bugs relating to permission prompts, and launched another round of
updates to PageInfo and Website Settings on Android.

Beyond the browser, our [web platform efforts](/Home/chromium-security/owp)
foster cross-vendor cooperation on developer-facing security features. The
W3C's[ WebAppSec](http://www.w3.org/2011/webappsec/) working group continues to
be a fairly productive venue for a number of important features: we've polished
the [Subresource
Integrity](https://w3c.github.io/webappsec/specs/subresourceintegrity/) spec and
shipped an implementation in Chrome 46, published first drafts of [Credential
Management](https://w3c.github.io/webappsec/specs/credentialmanagement/) and
[Entry Point Regulation](https://w3c.github.io/webappsec/specs/epr/), continue
to push [Content Security Policy Level 2](http://www.w3.org/TR/CSP2/) and [Mixed
Content](http://www.w3.org/TR/mixed-content/) towards "Recommendation" status,
and fixed some [longstanding](https://crbug.com/483458)
[bugs](https://crbug.com/508310) with our [Referrer
Policy](http://www.w3.org/TR/referrer-policy/) implementation.

Elsewhere, we've started prototyping [Per-Page
Suborigins](https://code.google.com/p/chromium/issues/detail?id=336894) with the
intent of bringing a concrete proposal to WebAppSec, published a new draft of
[First-Party-Only
cookies](https://tools.ietf.org/html/draft-west-first-party-cookies-03) (and are
working through some [infrastructure
improvements](https://docs.google.com/document/d/19NACt9PXOUTJi60klT2ZGcFlgHM5wM1Owtcw2GQOKPI/edit)
so we can ship them), and [poked at sandboxed
iframes](https://wiki.whatwg.org/index.php?title=Iframe_sandbox_improvments) to
make it possible to sandbox ads.

We see migration to HTTPS as foundational to any security whatsoever
([and](https://tools.ietf.org/html/rfc7258)
[we're](https://www.iab.org/2014/11/14/iab-statement-on-internet-confidentiality/)
[not](https://www.iab.net/iablog/2015/03/adopting-encryption-the-need-for-https.html)
[the](https://w3ctag.github.io/web-https/) [only](https://https.cio.gov/)
[ones](https://blog.mozilla.org/security/2015/04/30/deprecating-non-secure-http/)),
so we're actively working to drive #MOARTLS across Google and the Internet at
large. As a small practical step on top of the [HTTPS webmasters fundamentals
section](https://developers.google.com/web/fundamentals/discovery-and-distribution/security-with-https/?hl=en),
we’ve added some functionality to [Webmaster
Tools](https://www.google.com/webmasters/tools/home?hl=en&pli=1) to provide
better assistance to webmasters when dealing with common errors in managing a
site over TLS (launching soon!). Also, we're now measuring the usage of
[pre-existing, powerful features on non-secure
origins](/Home/chromium-security/deprecating-powerful-features-on-insecure-origins),
and are now printing deprecation warnings in the JavaScript console. Our
ultimate goal is to make all powerful features, such as Geolocation and
getUserMedia, available only to secure origins.

For more thrilling security updates and feisty rants, subscribe to
[security-dev@chromium.org](mailto:security-dev@chromium.org).

Happy Hacking,

Parisa, on behalf of Chrome Security

## Q1 2015

Hello from the Chrome Security Team!

For those that don’t know us already, we do stuff to help make Chrome the[ most
secure platform to browse the Internet](/Home/chromium-security). Here’s a recap
of some work from last quarter:

The[ Bugs--](/Home/chromium-security/bugs) effort aims to find (and exterminate)
security bugs. Last quarter, we rewrote our IPC fuzzer, which resulted in
[lots](https://code.google.com/p/chromium/issues/list?can=1&q=linux_asan_chrome_ipc+type%3Dbug-security+-status%3AWontFix%2CDuplicate+&sort=-id+-security_severity+-secseverity+-owner+-modified&colspec=ID+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner+Reporter&x=m&y=releaseblock&cells=tiles)
[more](https://code.google.com/p/chromium/issues/list?can=1&q=linux_asan_chrome_ipc_32bit+type%3Dbug-security+-status%3AWontFix%2CDuplicate&sort=-id+-security_severity+-secseverity+-owner+-modified&colspec=ID+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner+Reporter&x=m&y=releaseblock&cells=tiles)
[bugs](https://code.google.com/p/chromium/issues/list?can=1&q=linux_msan_chrome_ipc+type%3Dbug-security+-status%3AWontFix%2CDuplicate&sort=-id+-security_severity+-secseverity+-owner+-modified&colspec=ID+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner+Reporter&x=m&y=releaseblock&cells=tiles)
discovered by [ClusterFuzz](/Home/chromium-security/bugs/using-clusterfuzz)! We
also expanded fuzzing platform support (Android Lollipop, Linux with Nvidia
GPU), added [archived
builds](http://storage.cloud.google.com/chrome-test-builds/media) for
[proprietary media codecs](http://www.chromium.org/audio-video) testing on all
platforms, and used more code annotations to find bugs (like
[this](https://code.google.com/p/chromium/issues/detail?id=468519&can=1&q=%22Container-overflow%20in%20%22%20type%3Dbug-security%20-status%3AWontFix%2CDuplicate&sort=-id%20-security_severity%20-secseverity%20-owner%20-modified&colspec=ID%20Pri%20Status%20Summary%20Modified%20OS%20M%20Security_severity%20Security_impact%20Owner%20Reporter)
or
[this](https://code.google.com/p/chromium/issues/detail?id=468406&can=1&q=%22Container-overflow%20in%20%22%20type%3Dbug-security%20-status%3AWontFix%2CDuplicate&sort=-id%20-security_severity%20-secseverity%20-owner%20-modified&colspec=ID%20Pri%20Status%20Summary%20Modified%20OS%20M%20Security_severity%20Security_impact%20Owner%20Reporter)).
We auto-add previous crash tests to our data corpus, which helps to catch
regressions even if a developer forgets to add a test
([example](https://code.google.com/p/chromium/issues/detail?id=478745)). We’ve
also started experimenting with enabling and leveraging code coverage
information from fuzzing. Contrary to what [some reports may
imply](http://secunia.com/resources/vulnerability-review/browser-security/), [we
don’t think vulnerability counting is a good standalone metric for
security](http://lcamtuf.blogspot.com/2010/05/vulnerability-databases-and-pie-charts.html),
and more bugs discovered internally
([653](https://code.google.com/p/chromium/issues/list?can=1&q=type%3Abug-security+label%3AClusterFuzz+opened-after%3A2014%2F1%2F1+opened-before%3A2015%2F1%2F1+-status%3AWontFix%2CDuplicate&sort=-id+-security_severity+-secseverity+-owner+-modified&colspec=ID+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner+Reporter&x=m&y=releaseblock&cells=tiles)
bugs in 2014 vs.
[380](https://code.google.com/p/chromium/issues/list?can=1&q=type%3Abug-security+label%3AClusterFuzz+opened-after%3A2013%2F1%2F1+opened-before%3A2014%2F1%2F1+-status%3AWontFix%2CDuplicate&sort=-id+-security_severity+-secseverity+-owner+-modified&colspec=ID+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner+Reporter&x=m&y=releaseblock&cells=tiles)
bugs in 2013), means more bugs fixed, means safer software! Outside of
engineering, inferno@ gave a talk at [nullcon](http://nullcon.net/) about Chrome
fuzzing
([slides](http://nullcon.net/website/archives/ppt/goa-15/analyzing-chrome-crash-reports-at-scale-by-abhishek-arya.pdf))
and we launched [never-ending
Pwnium](http://blog.chromium.org/2015/02/pwnium-v-never-ending-pwnium.html) with
a rewards pool up to $∞ million!

Bugs still happen, so our[ Guts](/Home/chromium-security/guts) effort builds in
multiple layers of defense. On Linux and Chrome OS, we did some work to improve
the seccomp-BPF compiler and infrastructure. On modern kernels, we [finally
completed the switch](https://crbug.com/312380) from the [setuid sandbox to a
new design using
](https://code.google.com/p/chromium/wiki/LinuxSandboxing#The_setuid_sandbox)[unprivileged
namespaces](https://code.google.com/p/chromium/wiki/LinuxSandboxing#User_namespaces_sandbox)[.
](https://code.google.com/p/chromium/wiki/LinuxSandboxing#The_setuid_sandbox)We’re
also working on a generic, re-usable sandbox API on Linux, which we hope can be
useful to other Linux projects that want to employ sandboxing. On Android, we’ve
been experimenting with single-threaded renderer execution, which can yield
performance and security benefits for Chrome. We’ve also been involved with the
ambitious [Mojo](/developers/design-documents/mojo) effort. On OSX, we [shipped
crashpad](https://groups.google.com/a/chromium.org/forum/#!topic/chromium-dev/6eouc7q2j_g)
(which was a necessary project to investigate those sometimes-security-relevant
crashes!). Finally, on Windows, the support to block
[Win32k](/developers/design-documents/sandbox#TOC-Process-mitigation-policies-Win32k-lockdown-)
system calls from renderers on Windows 8 and above is now enabled on Stable -
and renderers on these systems are also running within [App
Containers](/developers/design-documents/sandbox#TOC-App-Container-low-box-token-)
on Chrome Beta, which blocks their access to the network. We also ensured all
Chrome allocations are safe - and use less memory (!) - by moving to the Windows
heap.

On our [Site Isolation](/developers/design-documents/site-isolation) project,
we’ve made progress on the underlying architecture so that complex pages are
correct and stable (e.g. rendering any combination of iframes, evaluating
renderer-side security checks, sending postMessage between subframes, keeping
script references alive). Great progress has also been made on session history,
DevTools, and test/performance infrastructure, and other teams have started
updating their features for out-of-process iframes after our [Site Isolation
Summit](https://www.youtube.com/playlist?list=PL9ioqAuyl6UJmC0hyI-k1wYW08O71lBn8).

Not all security problems can be solved in[ Chrome’s
guts](/Home/chromium-security/guts), so[ we work on making security more
user-friendly](/Home/chromium-security/enamel) too. In an effort to determine
the causes of SSL errors, we’ve added a new checkbox on SSL warnings that allows
users to send us invalid certificate chains for analysis. We’ve started looking
at the data, and in the coming months we plan to introduce new warnings that
provide specific troubleshooting steps for common causes of spurious warnings.
We also recently launched the new permissions bubble UI, which solves some of
the problems we had with permissions infobars (like better coalescing of
multiple permission requests). And for our Android users, we recently revamped
PageInfo and Site Settings, making it easier than ever for people to manage
their permissions. Desktop updates to PageInfo and Site Settings are in
progress, too. Finally, we just launched a new extension, [Chrome User
Experience
Surveys](https://chrome.google.com/webstore/detail/chrome-user-experience-su/hgimloonaobbeeagepickockgdcghfnn?utm_source=newsletter&utm_medium=security&utm_campaign=CUES%20extension),
which asks people for in-the-moment feedback after they use certain Chrome
features. If you’re interested in helping improve Chrome, you should try it out!

Beyond the browser, our [web platform efforts](/Home/chromium-security/owp)
foster cross-vendor cooperation on developer-facing security features. We're
working hard with the good folks in the W3C's
[WebAppSec](http://www.w3.org/2011/webappsec/) working group to make progress on
a number of specifications: [CSP 2](http://www.w3.org/TR/CSP2/) and [Mixed
Content](http://www.w3.org/TR/mixed-content/) have been published as Candidate
Recommendations, [Subresource Integrity](http://www.w3.org/TR/SRI/) is
implemented behind a flag and the spec is coming together nicely, and we've
fixed a number of [Referrer Policy](http://www.w3.org/TR/referrer-policy/)
issues. [First-Party-Only
Cookies](https://tools.ietf.org/html/draft-west-first-party-cookies) are just
about ready to go, and [Origin
Cookies](https://tools.ietf.org/html/draft-west-origin-cookies) are on deck.

We see migration to HTTPS as foundational to any security whatsoever
([and](https://tools.ietf.org/html/rfc7258)
[we're](https://www.iab.org/2014/11/14/iab-statement-on-internet-confidentiality/)
[not](https://www.iab.net/iablog/2015/03/adopting-encryption-the-need-for-https.html)
[the](https://w3ctag.github.io/web-https/) [only](https://https.cio.gov/)
[ones](https://blog.mozilla.org/security/2015/04/30/deprecating-non-secure-http/)),
so we're actively working to [define the properties of secure
contexts](https://w3c.github.io/webappsec/specs/powerfulfeatures/), [deprecate
powerful features on insecure
origins](/Home/chromium-security/deprecating-powerful-features-on-insecure-origins),
and to make it simpler for developers to [Upgrade Insecure
Requests](https://w3c.github.io/webappsec/specs/upgrade/) on existing sites.

For more thrilling security updates and feisty rants, subscribe to
[security-dev@chromium.org](mailto:security-dev@chromium.org).

Happy Hacking,

Parisa, on behalf of Chrome Security

P.S. Go here to travel back in time and view previous [Chrome security quarterly
updates](/Home/chromium-security/quarterly-updates).

## Q4 2014

Hello from the Chrome Security Team!

For those that don’t know us already, we do stuff to help make Chrome the[ most
secure platform to browse the
Internet](http://www.chromium.org/Home/chromium-security). Here’s a recap of
some work from last quarter:

The[ Bugs--](/Home/chromium-security/bugs) effort aims to find (and exterminate)
security bugs. Last quarter, we incorporated more [coverage
data](https://code.google.com/p/address-sanitizer/wiki/AsanCoverage) into our
[ClusterFuzz dashboard](https://cluster-fuzz.appspot.com/#coverage), especially
for [Android](https://cluster-fuzz.appspot.com/coverage?platform=ANDROID). With
this, we hope to optimize our test cases and improve fuzzing efficiency. We also
incorporated 5 new fuzzers from the external research community as part of the
fuzzer reward program. This has resulted in
[33](https://code.google.com/p/chromium/issues/list?can=1&q=decoder_langfuzz+Type%3DBug-Security+-status%3ADuplicate&colspec=ID+Pri+M+Week+ReleaseBlock+Cr+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=tiles)
[new](https://code.google.com/p/chromium/issues/list?can=1&q=therealholden_worker+Type%3DBug-Security+-status%3ADuplicate&colspec=ID+Pri+M+Week+ReleaseBlock+Cr+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=tiles)
[security](https://code.google.com/p/chromium/issues/list?can=1&q=cdiehl_peach+Type%3DBug-Security+-status%3ADuplicate&colspec=ID+Pri+M+Week+ReleaseBlock+Cr+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=tiles)
vulnerabilities. Finally, we wrote a multi-threaded test case minimizer from
scratch based on [delta debugging](https://www.st.cs.uni-saarland.de/dd/) (a
long-standing request from blink devs!) which produces clean, small,
reproducible test cases. In reward program news, we've paid over $1.6 million
for externally reported Chrome bugs since 2010 ([$4 million total across
Google](http://googleonlinesecurity.blogspot.com/2015/01/security-reward-programs-year-in-review.html)).
In 2014, over 50% of reward program bugs were found and fixed before they hit
the stable channel, protecting our main user population. Oh, and in case you
didn’t notice, the [rewards we’re paying out for vulnerabilities went
up](http://googleonlinesecurity.blogspot.com/2014/09/fewer-bugs-mo-money.html)
again.

Bugs still happen, so our[ Guts](/Home/chromium-security/guts) effort builds in
multiple layers of defense. We’re most excited about progress toward a tighter
sandbox for Chrome on Android (via
[seccomp-bpf](https://www.kernel.org/doc/Documentation/prctl/seccomp_filter.txt)),
which required landing seccomp-bpf support in Android and [enabling TSYNC on all
Chrome OS](https://codereview.chromium.org/759343002/) and Nexus kernels. We’ve
continued to improve our Linux / Chrome OS sandboxing by (1) adding full
cross-process interaction restrictions at the BPF sandbox level, (2) making API
improvements and some code refactoring of //sandbox/linux, and (3) implementing
a more powerful policy system for the GPU sandbox.

After ~2 years of work on [Site
Isolation](/developers/design-documents/site-isolation), we’re happy to announce
that [out-of-process iframes](/developers/design-documents/oop-iframes) are
working well enough that some Chrome features have [started updating to support
them](https://docs.google.com/document/d/1dCR2aEoBJj_Yqcs6GuM7lUPr0gag77L5OSgDa8bebsI/edit?usp=sharing)!
These include autofill (done), accessibility (nearly done), &lt;webview&gt;
(prototyping), devtools, and extensions. We know how complex a rollout this will
be, and we’re ready with testing infrastructure and
[FYI](http://build.chromium.org/p/chromium.fyi/builders/Site%20Isolation%20Linux)
[bots](http://build.chromium.org/p/chromium.fyi/builders/Site%20Isolation%20Win).
As we announced at our recent Site Isolation Summit
([video](https://www.youtube.com/playlist?list=PL9ioqAuyl6UJmC0hyI-k1wYW08O71lBn8),
[slides](https://docs.google.com/presentation/d/10HTTK4dsxO5p6FcpEOq8EkuV4yiBx2n6dBki8cqDWyo/edit?usp=sharing)),
our goal for Q1 is to finish up OOPIF support with the help of all of Chrome.

Not all security problems can be solved in [Chrome’s
Guts](/Home/chromium-security/guts), so[ we work on making security more
user-friendly](/Home/chromium-security/enamel) too. For the past few months,
we’ve been looking deeper into the causes of SSL errors by looking at UMA stats
and monitoring user help forums. One source of SSL errors is system clocks with
the wrong time, so we landed a more informative error message in Chrome 40 to
let users know they need to fix their clock. We’ve also started working on a
warning interstitial for [captive
portals](http://en.wikipedia.org/wiki/Captive_portal) to distinguish those SSL
errors from the rest. Finally, we [proposed a plan for browsers to migrate their
user interface from marking insecure origins (i.e. HTTP) as explicitly
insecure](/Home/chromium-security/marking-http-as-non-secure); the [initial
discussion](https://groups.google.com/a/chromium.org/forum/#!topic/security-dev/DHQLv76QaEM%5B1-25-false%5D)
and [external attention](http://www.bbc.com/news/technology-30505970) has been
generally positive.

Over the past few years, we’ve worked on a bunch of isolated projects to push
security on the Open Web Platform forward and make it possible for developers to
write more secure apps. We recognized we can move faster if we get some of the
team fully dedicated to this work, so we formed a new group that will focus on
[web platform efforts](/Home/chromium-security/owp).

As usual, for more thrilling security updates and feisty rants, subscribe to
[security-dev@chromium.org](mailto:security-dev@chromium.org).

To a safer web in 2015!

Parisa, on behalf of Chrome Security

## Q3 2014

Hello from the Chrome Security Team!

For those that don’t know us already, we do stuff to help make Chrome the[ most
secure platform to browse the
Internet](http://www.chromium.org/Home/chromium-security). Here’s a recap of
some work from last quarter:

The[ Bugs--](/Home/chromium-security/bugs) effort aims to find (and exterminate)
security bugs. We increased Clusterfuzz cores across all desktop platforms (Mac,
Android, Windows, and Linux), resulting in
[155](https://code.google.com/p/chromium/issues/list?can=1&q=Type%3DBug-Security+label%3AClusterFuzz+opened-after%3A2014%2F7%2F1+opened-before%3A2014%2F9%2F31+-status%3AWontFix%2CDuplicate&sort=-security_severity+-secseverity+-owner+-modified&colspec=ID+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner+Reporter&x=m&y=releaseblock&cells=tiles)
security and
[275](https://code.google.com/p/chromium/issues/list?can=1&q=Type%3DBug+label%3AClusterFuzz+opened-after%3A2014%2F7%2F1+opened-before%3A2014%2F9%2F31+-status%3AWontFix%2CDuplicate&sort=-security_severity+-secseverity+-owner+-modified&colspec=ID+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner+Reporter&x=m&y=releaseblock&cells=tiles)
functional bugs since last update! We also started fuzzing D-Bus system services
on Chrome OS, which is our first attempt at leveraging Clusterfuzz for the
operating system. One of the common security pitfalls in C++ is bad casting
(often rooted in aggressive polymorphism). To address, [one of our
interns](http://www.cc.gatech.edu/~blee303/) tweaked [UBSAN (Undefined Behavior
Sanitizer)
vptr](https://drive.google.com/file/d/0Bxvv8gduedamTEJCUlN6eERtWUE/view?usp=sharing)
to detect bad-casting at runtime, which resulted in [11 new security
bugs](https://code.google.com/p/chromium/issues/list?can=1&q=Type%3DBug-Security+linux_ubsan_vptr_chrome+-status%3ADuplicate%2CWontFix&sort=-security_severity+-secseverity+-owner+-modified&colspec=ID+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner+Reporter&x=m&y=releaseblock&cells=tiles&)!
We’ve continued to collaborate with external researchers on new fuzzing
techniques to find bugs in V8, Pdfium, Web Workers, IDB, and more. Shout out to
attekett, cloudfuzzer, decoder.oh, and therealholden for their attention and
bugs over the past quarter!

Finding bugs is only half the battle, so we also did a few things to make it
easier to get security bugs ==fixed==, including (1) a new [security
sheriff](/Home/chromium-security/security-sheriff)
[dashboard](https://cluster-fuzz.appspot.com/#sheriff) and (2) contributing to
the [FindIt
project](https://chromium.googlesource.com/chromium/src.git/+/lkgr/tools/findit/),
which helps narrow down suspected CL(s) for a crash (given a regression range
and stacktrace), thereby saving manual triage cycles.

Bugs still happen, so our[ Guts](/Home/chromium-security/guts) effort builds in
multiple layers of defense. We did a number of things to push
[seccomp-bpf](http://blog.chromium.org/2012/11/a-safer-playground-for-your-linux-and.html)
onto more platforms and architectures, including: (1) adding support for
[MIPS](https://code.google.com/p/chromium/issues/detail?id=369594) and
[ARM64](https://code.google.com/p/chromium/issues/detail?id=355125), (2) adding
a [new capability](https://lkml.org/lkml/2014/7/17/844) to initialize
seccomp-bpf in the presence of threads (bringing us a big step closer to a
stronger sandbox on Android), (3) [general tightening of the
sandboxes](https://crbug.com/413855), and (4) writing a [domain-specific
language to better express BPF
policies](https://drive.google.com/file/d/0B9LSc_-kpOQPVHhvcVBza3NWR0k/view). We
also helped ensure a [safe launch of Android apps on Chrome
OS](http://chrome.blogspot.com/2014/09/first-set-of-android-apps-coming-to.html),
and continued sandboxing new system services.

On Windows, we [launched Win64 to
Stable](http://blog.chromium.org/2014/08/64-bits-of-awesome-64-bit-windows_26.html),
giving users a safer, speedier, and more stable version of Chrome! On Windows 8,
we added Win32k system call filtering behind a
[switch](https://code.google.com/p/chromium/codesearch#chromium/src/content/public/common/content_switches.cc&sq=package:chromium&l=966),
further reducing the kernel attack surface accessible from the renderer. We also
locked down the [alternate
desktop](http://www.chromium.org/developers/design-documents/sandbox#TOC-The-alternate-desktop)
sandbox tokens and refactored the sandbox startup to cache tokens, which
improves new tab responsiveness.

Finally, work continues on [site
isolation](http://www.chromium.org/developers/design-documents/site-isolation).
Over the past few months, we’ve started creating RemoteFrames in Blink's frame
tree to support out-of-process iframes (OOPIF) and got
[Linux](http://build.chromium.org/p/chromium.fyi/builders/Site%20Isolation%20Linux)
and
[Windows](http://build.chromium.org/p/chromium.fyi/builders/Site%20Isolation%20Win)
FYI bots running tests with --site-per-process. We’ve also been working with the
[Accessibility](https://groups.google.com/a/chromium.org/forum/#!forum/chromium-accessibility)
team as our guinea pig feature to support OOPIF, and since that work is nearly
done, we’re reaching out to more teams over the next few months to update their
features (see our [FAQ about updating
features](https://docs.google.com/a/chromium.org/document/d/1Iqe_CzFVA6hyxe7h2bUKusxsjB6frXfdAYLerM3JjPo/edit)).

Not all security problems can be solved in[ Chrome’s
guts](/Home/chromium-security/guts), so[ we work on making security more
user-friendly](/Home/chromium-security/enamel) too. SSL-related warnings are
still a major source of user pain and confusion. Over the past few months, we’ve
been focused on determining the causes of false positive SSL errors (via adding
UMA stats for known client / server errors) and investigating
[pinning](/Home/chromium-security/education/tls#TOC-Certificate-Pinning)
violation reports. We’ve also been [experimenting with cert memory
strategies](https://codereview.chromium.org/369703002/) and integrating relevant
detail when we detect a (likely) benign SSL error due to captive portal or a bad
clock.

Developers are users too, so we know it’s important to support new web security
features and ensure new features are safe to use by default. In that vein, we
recently landed a first pass at [subresource
integrity](https://w3c.github.io/webappsec/specs/subresourceintegrity/)
[support](https://codereview.chromium.org/566083003/) behind a flag (with
[useful console errors](https://codereview.chromium.org/596043003/)), we’re
[shipping](https://groups.google.com/a/chromium.org/d/msg/blink-dev/wToP6b04zVE/imuPatGy3awJ)
most of [CSP 2](https://w3c.github.io/webappsec/specs/content-security-policy/)
in M40, we’ve continued to [tighten
up](https://groups.google.com/a/chromium.org/d/msg/blink-dev/Uxzvrqb6IeU/9FAie9Py4cIJ)
handling of [mixed
content](https://w3c.github.io/webappsec/specs/mixedcontent/), and are working
to define and implement [referrer
policies](https://w3c.github.io/webappsec/specs/referrer-policy/). We’ve also
been helping on some security consulting for [Service
Worker](/blink/serviceworker); kudos to the team for making changes to [handle
plugins more
securely](https://code.google.com/p/chromium/issues/detail?id=413094), [restrict
usage to secure origins](http://crbug.com/394213), and for addressing some
memory caching issues. If you want to learn more about what’s going on in the
Blink Security world, check out the
[Blink-SecurityFeature](https://code.google.com/p/chromium/issues/list?q=label:Cr-Blink-SecurityFeature)
label.

And then there’s other random things, like ad-hoc hunting for security bugs
(e.g. [local privilege escalation bug in
pppd](https://code.google.com/p/chromium/issues/detail?id=390709)), [giving
Chromebooks](http://www.washingtonpost.com/blogs/the-switch/wp/2014/08/18/finding-a-safe-space-for-kids-to-hack/)
to [kids at
DEFCON](https://www.flickr.com/photos/asirap/sets/72157645916802437), and
various artistic endeavors, like [color-by-risk
diagramming](https://docs.google.com/a/chromium.org/drawings/d/1TuECFL9K7J5q5UePJLC-YH3satvb1RrjLRH-tW_VKeE/edit)
and [security-inspired
fashion](https://www.flickr.com/photos/asirap/14798014040/).

For more thrilling security updates and feisty rants, subscribe to
[security-dev@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/security-dev).

Happy Hacking (and Halloween),

Parisa, on behalf of Chrome Security

## Q2 2014

Hello from the Chromium Security Team!

For those that don’t know us already, we do stuff to help make Chrome the[ most
secure platform to browse the
Internet](http://www.chromium.org/Home/chromium-security). Here’s a recap of
some work from last quarter:

One of our primary responsibilities is security **adviser**, and the main way we
do this is via security reviews. A few weeks ago, jschuh@ announced [a new and
improved security review
process](http://www.chromium.org/Home/chromium-security/security-reviews) that
helps teams better assess their current security posture and helps our team
collect more meaningful data about Chrome engineering. All features for M37 went
through the new process, and we’ll be shepherding new projects and launches
through this process going forward.

The[ Bugs--](/Home/chromium-security/bugs) effort aims to find (and exterminate)
security bugs. One of our best ways of finding bugs and getting them fixed
quickly is fuzz testing via [ClusterFuzz](https://cluster-fuzz.appspot.com/).
This quarter, we started fuzzing Chrome on Mac OS (extending the existing
platform coverage on Windows, Linux, and Android). We also added [code coverage
stats](https://cluster-fuzz.appspot.com/#coverage) to the ClusterFuzz UI, which
some teams have been finding helpful as a complement to their QA testing, as
well as [fuzzer stats](https://cluster-fuzz.appspot.com/#fuzzerstats), which [V8
team now
checks](https://cluster-fuzz.appspot.com/fuzzerstats?fuzzer_name=stgao_chromebot2&job_type=linux_asan_chrome_v8&last_n_revisions=30)
in new rollouts. Finally, we added some new fuzzers (WebGL, GPU commands) and
integrated a number of memory debugging tools to find new classes of bugs (e.g.
[AddressSanitizer](https://code.google.com/p/address-sanitizer/) on Windows
found
[22](https://code.google.com/p/chromium/issues/list?can=1&q=type%3Abug-security+%22windows_asan_chrome%22&sort=-security_severity+-secseverity+-owner+-modified&colspec=ID+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner+Reporter&x=m&y=releaseblock&cells=tiles)
bugs, [Dr. Memory](http://www.drmemory.org/) on Windows found
[1](https://code.google.com/p/chromium/issues/list?can=2&q=Stability%3DMemory-DrMemory+label%3Aclusterfuzz&colspec=ID+Pri+M+Iteration+ReleaseBlock+Cr+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=tiles)
bug,
[MemorySanitizer](https://code.google.com/p/memory-sanitizer/wiki/MemorySanitizer)
on Linux found
[146](https://code.google.com/p/chromium/issues/list?can=1&q=label%3AClusterFuzz+Stability%3DMemory-MemorySanitizer+&sort=-security_severity+-secseverity+-owner+-modified&colspec=ID+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner+Reporter&x=m&y=releaseblock&cells=tiles)
bugs, and
[LeakSanitizer](https://code.google.com/p/address-sanitizer/wiki/LeakSanitizer)
on Linux found
[18](https://code.google.com/p/chromium/issues/list?can=1&q=label%3AClusterFuzz+Stability%3DMemory-LeakSanitizer+&sort=-security_severity+-secseverity+-owner+-modified&colspec=ID+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner+Reporter&x=m&y=releaseblock&cells=tiles)
bugs).

Another source of security bugs is our [vulnerability reward
program](/Home/chromium-security/vulnerability-rewards-program), which saw a
quiet quarter: only 32 reports opened in Q2 (lowest participation in 12 months)
and an average payout of $765 per bug (lowest value in 12 months). This trend is
likely due to (1) fuzzers, both internal and external, finding over 50% of all
reported bugs in Q2, (2) a reflection of both the increasing difficulty of
finding bugs and outdated reward amounts being less competitive, and (3)
researcher fatigue / lack of interest or stimulus. Plans for Q3 include
reinvigorating participation in the rewards program through a more generous
reward structure and coming up with clever ways to keep researchers engaged.

Outside of external bug reports, we spent quite a bit of time [improving the
security posture of Pdfium](/Home/chromium-security/pdfium-security) (Chrome's
[recently opensourced](http://blog.foxitsoftware.com/?p=641) PDF renderer) via
finding / fixing
~[150](https://code.google.com/p/chromium/issues/list?can=1&q=type%3Abug-security+Cr%3DInternals-Plugins-PDF+closed-after%3A2014%2F4%2F1+closed-before%3A2014%2F7%2F31&sort=-security_severity+-secseverity+-owner+-modified&colspec=ID+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner+Reporter&x=m&y=releaseblock&cells=tiles)
bugs, removing risky code (e.g. [custom
allocator](https://pdfium.googlesource.com/pdfium/+/3522876d5291922ddc62bf1b70d02743b0850673)),
and using [secure integer
library](https://code.google.com/p/chromium/codesearch#chromium/src/base/numerics/&ct=rc&cd=1&q=numerics&sq=package:chromium)
for overflow checks. Thanks to ifratric@, mjurczyk@, and gynvael@ for their PDF
fuzzing help!

Bugs still happen, so our [Guts](/Home/chromium-security/guts) effort builds in
multiple layers of defense. We did lots of sandboxing work across platforms last
quarter. On Mac OS, rsesek@ started working on a brand new [bootstrap sandbox
for
OSX](https://docs.google.com/a/chromium.org/document/d/108sr6gBxqdrnzVPsb_4_JbDyW1V4-DRQUC4R8YvM40M/edit)
(//sandbox/mac) and on Android, he got a proof-of-concept renderer running under
[seccomp-bpf](http://lwn.net/Articles/475043/). On Linux and Chrome OS, we
continued to improve the sandboxing testing framework and wrote dozens of new
tests; all our security tests are now running on the Chrome OS BVT. We also
refactored all of NaCl-related “outer” sandboxing to support a new and faster
Non-SFI mode for [NaCl](https://developer.chrome.com/native-client). This is
being used to run Android apps on Chrome, as you may have seen [demoed at Google
I/O](https://www.google.com/events/io).

After many months of hard work, we’re ecstatic to announce that we [released
Win64 on dev and
canary](http://blog.chromium.org/2014/06/try-out-new-64-bit-windows-canary-and.html)
to our Windows 7 and Windows 8 users. This release takes advantage of [High
Entropy
ASLR](http://blogs.technet.com/b/srd/archive/2013/12/11/software-defense-mitigating-common-exploitation-techniques.aspx)
on Windows 8, and the extra bits help improve the effectiveness of heap
partitioning and mitigate common exploitation techniques (e.g. JIT spraying).
The Win64 release also reduced ~⅓ of the crashes we were seeing on Windows, so
it’s more stable too!

Finally, work continues on [site
isolation](http://www.chromium.org/developers/design-documents/site-isolation):
lots of code written / rewritten / rearchitected and unknown unknowns discovered
along the way. We're close to having "remote" frames for each out-of-process
iframe, and you can now see subframe processes in Chrome's Task Manager when
visiting a test page like
[this](http://csreis.github.io/tests/cross-site-iframe.html) with the
--site-per-process flag.

Not all security problems can be solved in[ Chrome’s
guts](/Home/chromium-security/guts), so[ we work on making security more
user-friendly](/Home/chromium-security/enamel) too. The themes of Q2 were SSL
and permissions. For SSL, we nailed down a new ["Prefer Safe Origins for
Powerful Features"
policy](/Home/chromium-security/prefer-secure-origins-for-powerful-new-features),
which we’ll transition to going forward; kudos to palmer@ and sleevi@ for
ironing out all the details and getting us to a safer default state. We’ve also
been trying to improve the experience of our SSL interstitial, which most people
ignore :-/ Work includes launching new UX for [SSL
warnings](https://test-sspev.verisign.com/) and incorporating captive portal
status (ongoing). Congrats to agl@ for launching
[boringssl](https://www.imperialviolet.org/2014/06/20/boringssl.html) - if
boring means avoiding Heartbleed-style hysteria, sounds good to us!

On the permissions front, we’re working on ways to give users more control over
application privileges, such as (1) reducing the number of install-time CRX
permissions, (2) running UX experiments on the effectiveness of permissions, and
(3) working on building a security and permissions model to bring native
capabilities to the web.

For more thrilling security updates and feisty rants, subscribe to
[security-dev@chromium.org](mailto:security-dev@chromium.org).

In the meantime, happy hacking!

Parisa, on behalf of Chrome Security

P.S. A big kudos to the V8 team, and jkummerow@ in particular, for their extra
security efforts this quarter! The team rapidly responded to and fixed a number
of security bugs on top of doing some security-inspired hardening of V8 runtime
functions.

## Q1 2014

Hello from the Chrome Security Team!

For those that don’t know us already, we help make Chrome the [most secure
platform to browse the
Internet](http://www.chromium.org/Home/chromium-security). In addition to
security reviews and consulting, running a [vulnerability reward
program](/Home/chromium-security/vulnerability-rewards-program), and dealing
with security surprises, we instigate and work on engineering projects that make
Chrome safer. Here’s a recap of some work from last quarter:

The [Bugs-- ](/Home/chromium-security/bugs)effort aims to find (and exterminate)
exploitable bugs. A major accomplishment from Q1 was getting
[ClusterFuzz](https://cluster-fuzz.appspot.com/) coverage for Chrome on Android;
we’re aiming to scale up resources from [a few devices on inferno@’s
desk](https://plus.sandbox.google.com/u/0/103970240746069356722/posts/LckbsWq6QFZ)
to 100 bots over the next few months. On the fuzzer front, mbarbella@ wrote a
new V8 fuzzer that helped shake out
[30+](https://code.google.com/p/chromium/issues/list?can=1&q=type%3Dbug-security+-status%3Aduplicate+mbarbella_js_mutation&sort=-security_severity+-secseverity+-owner+-modified&colspec=ID+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner+Reporter&x=m&y=releaseblock&cells=tiles)
[bugs](https://code.google.com/p/chromium/issues/list?can=1&q=type%3Dbug-security+-status%3Aduplicate+mbarbella_js_mutation_test&sort=-security_severity+-secseverity+-owner+-modified&colspec=ID+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner+Reporter&x=m&y=releaseblock&cells=tiles);
kudos to the V8 team for being so proactive at fixing these issues and
prioritizing additional proactive security work this quarter. Spring welcomed a
hot new line of PoC exploits at [Pwn2Own](http://www.pwn2own.com/) and [Pwnium
4](http://blog.chromium.org/2014/01/show-off-your-security-skills.html):
highlights included a classic ensemble of overly broad IPC paired with a Windows
“feature,” and a bold chain of 5 intricate bugs for persistent system compromise
on Chrome OS; more details posted soon [here](/Home/chromium-security/bugs).
Beyond exploit contests, we’ve rewarded $52,000 for reports received this year
(from 16 researchers for 23 security bugs) via our ongoing [vulnerability reward
program](/Home/chromium-security/vulnerability-rewards-program). We also started
rewarding researchers for bugs in [Chrome extensions developed "by
Google.”](http://googleonlinesecurity.blogspot.com/2014/02/security-reward-programs-update.html)
Outside of finding and fixing bugs, jschuh@ landed [a safe numeric
class](https://code.google.com/p/chromium/issues/detail?id=332611) to help
prevent arithmetic overflow bugs from being introduced in the first place; use
it and you'll [sleep better](https://xkcd.com/571/) too!

Bugs still happen, so we build in multiple layers of defense. One of our most
common techniques is [sandboxing](/Home/chromium-security/guts), which helps to
reduce the impact of any single bug. Simple in theory, but challenging to
implement, maintain, and improve across all platforms. On Linux and Chrome OS,
we spent a lot of the quarter paying back technical debt: cleaning up the GPU
sandbox, writing and fixing tests, and replacing the setuid sandbox. On Android,
we reached consensus with the Android Frameworks team on a path forward for
seccomp-bpf sandboxing for Clank. We've started writing the CTS tests to verify
this in Android, landed the baseline policy in upstream Clankium, and are
working on the required [upstream Linux Kernel
changes](https://lkml.org/lkml/2014/1/13/795) to be incorporated into Chrome
Linux, Chrome OS, and Android L. The [site isolation
project](http://www.chromium.org/developers/design-documents/site-isolation)
(i.e. sandboxing at the site level) landed a usable cross-process iframe
implementation behind --site-per-process, which supports user interaction,
nested iframes (one per doc), sad frame, and basic DevTools support. Major
refactoring of Chrome and Blink, performance testing, and working with teams
that need to update for site isolation continues this quarter. On Windows, we
shipped Win64 canaries, landed code to sandbox the auto update mechanism, and
improved the existing sandboxing, reducing the win32k attack surface by ~30%.
Thanks to the Windows Aura team, we’ve also made tremendous progress on
disabling win32k entirely in the Chrome sandbox, which will eventually eliminate
most Windows-specific sandbox escapes.

Not all security can be solved in [Chromium’s
Guts](/Home/chromium-security/guts), so [we work on making security more
user-friendly](/Home/chromium-security/enamel) too. We finally landed the
controversial change to [remember passwords, even when
autocomplete='off'](https://code.google.com/p/chromium/issues/detail?id=177288)
in
[M34](http://blog.chromium.org/2014/02/chrome-34-responsive-images-and_9316.html),
which is a small, but [significant change to return control back to the
user](/Home/chromium-security/security-faq). We also made some [tweaks to the
malware download UX in
M32](https://docs.google.com/a/google.com/presentation/d/16ygiQS0_5b9A4NwHxpcd6sW3b_Up81_qXU-XY86JHc4/edit#slide=12);
previously users installed ~29% of downloads that were known malware, and that
number is now down to &lt;5%! We’ve recently been thinking a lot about how to
improve the security of Chrome Extensions and Apps, including experimenting with
several changes to the permission dialog to see if we can reduce the amount of
malicious crx installed by users without reducing the amount of non-malicious
items. Separately, we want to make it easier for developers to write secure
APIs, so meacer@ wrote up some [security
tips](https://docs.google.com/document/d/1RamP4-HJ7GAJY3yv2ju2cK50K9GhOsydJN6KIO81das/pub)
to help developers avoid common abuse patterns we’ve identified from bad actors.

Finally, since [Heartbleed](http://heartbleed.com/) is still on the forefront of
many minds, a reminder that Chrome and Chrome OS [were not directly
affected](http://googleonlinesecurity.blogspot.com/2014/04/google-services-updated-to-address.html).
And if you're curious about how and why Chrome does SSL cert revocation the way
it does, agl@ wrote a great
[post](https://www.imperialviolet.org/2014/04/19/revchecking.html) explaining
that too.

For more thrilling security updates and feisty rants, subscribe to[
security-dev@chromium.org](mailto:security-dev@chromium.org).

Happy Hacking,
Parisa, on behalf of Chrome Security

## Q4 2013

Hello from the Chrome Security Team!
For those that don’t know us already, we help make Chromium the [most secure
browsing platform in the
market](http://www.chromium.org/Home/chromium-security). In addition to security
reviews and consulting, running a [vulnerability reward
program](/Home/chromium-security/vulnerability-rewards-program), and dealing
with security surprises, we instigate and work on engineering projects that make
Chrome more secure.
The end of last year flew by, but here are a couple of things we’re most proud
of from the last quarter of 2013:
Make security more usable: We made a number of changes to the malware download
warning to discourage users from installing malware. We also worked on a
reporting feature that lets users upload suspicious files to [Safe
Browsing](http://www.google.com/transparencyreport/safebrowsing/), which will
help Safe Browsing catch malicious downloads even faster.
Since PDFs are a common vehicle for exploit delivery, we’ve modified PDF
handling in Chrome so that they're all opened in Chrome’s PDF viewer by default.
This is a huge security win because we believe Chrome’s PDF viewer is the
safest, most hardened, and security-tested viewer available. [Malware via
Microsoft .docs are also
common](https://www.eff.org/deeplinks/2014/01/vietnamese-malware-gets-personal),
so we’re eagerly awaiting the day we can open Office Docs in
[Quickoffice](https://support.google.com/quickoffice/answer/2986862?hl=en) by
default.
Find (and fix) more security bugs: We recently welcomed a new member to the
team,
[Sheriffbot](https://code.google.com/p/chromium/issues/list?can=1&q=type%3Abug-security+commentby%3Aclusterfuzz%40chromium.org+-reporter%3Aclusterfuzz%40chromium.org&sort=-id+-security_severity+-secseverity+-owner+-modified&colspec=ID+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner+Reporter&cells=tiles).
He’s already started making the [mortal security
sheriffs’](/Home/chromium-security/security-sheriff) lives easier by finding new
owners, adding Cr- area labels, helping apply and fix bug labels, and reminding
people about open security bugs they have assigned to them.
Our fuzzing mammoth, [ClusterFuzz](https://cluster-fuzz.appspot.com/), is now
fully supported on Windows and has helped find
[32](https://code.google.com/p/chromium/issues/list?can=1&q=type:bug-security%20label:Hotlist-SyzyASAN%20label:ClusterFuzz&sort=-id+-security_severity+-secseverity+-owner+-modified&colspec=ID%20Pri%20Status%20Summary%20Modified%20OS%20M%20Security_severity%20Security_impact%20Owner%20Reporter)
new bugs. We’ve added a bunch of new fuzzers to cover Chromium IPC (5 high
severity bugs), networking protocols (1 critical severity bug from a certificate
fuzzer, 1 medium severity bug from an HTTP protocol fuzzer), and WebGL (1 high
severity bug in Angle). Want to write a
[fuzzer](http://en.wikipedia.org/wiki/Fuzz_testing) to add security fuzzing
coverage to your code? Check out the [ClusterFuzz
documentation](/Home/chromium-security/bugs/using-clusterfuzz), or get in touch.
In November, we helped sponsor a [Pwn2Own contest at the PacSec conference in
Tokyo](http://h30499.www3.hp.com/t5/HP-Security-Research-Blog/Mobile-Pwn2Own-2013/ba-p/6202185#.Ut6t45DTm91).
Our good friend, Pinkie Pie, exploited an integer overflow in V8 to get reliable
code execution in the renderer, and then exploited a bug in a Clipboard IPC
message to get code execution in the browser process (by spraying multiple
gigabytes of shared memory). We’ll be publishing a full write-up of the exploit
[on our site](/Home/chromium-security/bugs) soon, and are starting to get
excited about our [upcoming
Pwnium](http://blog.chromium.org/2014/01/show-off-your-security-skills.html) in
March.
Secure by default, defense in depth: In [Chrome
32](http://googlechromereleases.blogspot.com/2014/01/stable-channel-update.html),
we started [blocking NPAPI by
default](http://blog.chromium.org/2013/09/saying-goodbye-to-our-old-friend-npapi.html)
and have plans to completely remove support by the end of the year. This change
significantly reduces Chrome’s exposure to browser plugin vulnerabilities. We
also implemented additional [heap partitioning for buffers and
strings](https://code.google.com/p/chromium/issues/detail?id=270531) in Blink,
which further mitigates memory exploitation techniques. Our Win64 port of
Chromium is now continuously tested on the main waterfall and is on track to
ship this quarter. Lastly, we migrated our Linux and Chrome OS sandbox to a new
policy format and did a lot of overdue sandbox code cleanup.
On our [site isolation](/developers/design-documents/site-isolation) project,
we’ve started landing infrastructure code on trunk to support out-of-process
iframes. We are few CLs away from having functional cross-process iframe behind
a flag and expect it to be complete by the end of January!
Mobile, mobile, mobile: We’ve started focusing more attention to hardening
Chrome on Android. In particular, we’ve been hacking on approaches for strong
sandboxing (e.g.
[seccomp-bpf](http://blog.chromium.org/2012/11/a-safer-playground-for-your-linux-and.html)),
adding [Safe Browsing](http://www.google.com/transparencyreport/safebrowsing/)
protection, and getting [ClusterFuzz](https://cluster-fuzz.appspot.com/) tuned
for Android.
For more thrilling security updates and feisty rants, catch ya on
[security-dev@chromium.org](mailto:security-dev@chromium.org).
Happy Hacking,
Parisa, on behalf of Chrome Security

## Q3 2013

An early
[boo](http://mountainbikerak.blogspot.com/2010/11/google-chrome-pumpkin.html)
and (late) quarter update from the Chrome Security Team!

For those that don’t know us already, we help make Chromium the [most secure
browsing platform in the
market](http://www.chromium.org/Home/chromium-security). In addition to security
reviews and consulting, running a [vulnerability reward
program](/Home/chromium-security/vulnerability-rewards-program), and dealing
with security surprises, we instigate and work on engineering projects that make
Chrome more secure.

Last quarter, we reorganized the larger team into 3 subgroups:

**Bugs--**, a group focused on finding security bugs, responding to them, and
helping get them fixed. The group is currently working on expanding
[Clusterfuzz](https://cluster-fuzz.appspot.com/) coverage to other platforms
(Windows and Mac), adding fuzzers to cover IPC, networking, and WebGL, adding
more [security
ASSERTS](http://svnsearch.org/svnsearch/repos/BLINK/search?logMessage=ASSERT_WITH_SECURITY_IMPLICATION)
to catch memory corruption bugs. They're also automating some of the grungy and
manual parts of being [security
sheriff](/Home/chromium-security/security-sheriff) to free up human cycles for
more exciting things.

Enamel, a group focused on usability problems that affect end user security or
the development of secure web applications. In the near-term, Enamel is working
on: improving the malware download warnings, SSL warnings, and extension
permission dialogs; making it safer to open PDFs and .docs in Chrome; and
investigating ways to combat popular phishing attacks.

Guts, a group focused on ensuring Chrome’s architecture is secure by design and
resilient to exploitation. Our largest project here is [site
isolation](/developers/design-documents/site-isolation), and in Q4, we’re aiming
to have a usable cross-process iframe implementation (behind a flag ;) Other
Guts top priorities include sandboxing work (stronger sandboxing on Android,
making Chrome OS’s
[seccomp-bpf](https://code.google.com/p/chromium/wiki/LinuxSandboxing#The_seccomp-bpf_sandbox)
easier to maintain and better tested), supporting [NPAPI
deprecation](http://blog.chromium.org/2013/09/saying-goodbye-to-our-old-friend-npapi.html),
launching 64bit Chrome for Windows, and Blink memory hardening (e.g. heap
partitioning).

Retrospectively, here are some of notable security wins from recent Chrome
releases:

In [Chrome
29](http://googlechromereleases.blogspot.com/2013/08/stable-channel-update.html),
we tightened up the sandboxing policies on Linux and added some defenses to the
Omaha (Chrome Update) plugin, which is a particularly exposed and attractive
target in Chrome. The first parts of Blink heap partition were released, and
we’ve had “backchannel” feedback that we made an impact on the greyhat exploit
market.

In [Chrome
30](http://googlechromereleases.blogspot.com/2013/10/stable-channel-update.html)
we fixed a load of security bugs! The spike in bugs was likely due to a few
factors: (1) we started accepting fuzzers (7 total) from invited external
researchers as part of a Beta extension to our vulnerability reward program
(resulting in
[26](https://code.google.com/p/chromium/issues/list?can=1&q=type%3Abug-security+label%3AExternal-Fuzzer-Contribution+-status%3AWontFix%2CDuplicate&sort=-security_severity+-secseverity+-owner+-modified&colspec=ID+Stars+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner&x=m&y=releaseblock&cells=tiles)
new bugs), (2) we [increased reward
payouts](http://googleonlinesecurity.blogspot.com/2013/08/security-rewards-at-google-two.html)
to spark renewed interest from the public, and (3) we found a bunch of new
buffer (over|under)flow and casting bugs ourselves by adding
[ASSERT_WITH_SECURITY_IMPLICATION](http://svnsearch.org/svnsearch/repos/BLINK/search?logMessage=ASSERT_WITH_SECURITY_IMPLICATION)s
in Blink. In M30, we also added [a new layer of
sandboxing](https://code.google.com/p/chromium/issues/detail?id=168812) to NaCl
on Chrome OS, with seccomp-bpf.

Last, but not least, we want to give a shout out to individuals outside the
security team that made an extraordinary effort to improve Chrome security:

*   Jochen Eisinger for redoing the pop-up blocker... so that it
            [actually blocks
            pop-ups](https://code.google.com/p/chromium/issues/detail?id=38458)
            (instead of hiding them). Beyond frustrating users, this bug was a
            security liability, but due to the complexity of the fix, languished
            in the issue tracker for years.
*   Mike West for his work on CSP, as well as tightening downloading of
            bad content types.
*   Avi Drissman for fixing a [longstanding bug where PDF password input
            was not
            masked](http://code.google.com/p/chromium/issues/detail?id=54748#c49).
*   Ben Hawkes and Ivan Fratic for finding
            [four](https://code.google.com/p/chromium/issues/list?can=1&q=label%3AWinFuzz)
            potentially exploitable Chrome bugs using WinFuzz.
*   Mateusz Jurczyk on finding ton of bugs in VP9 video decoder.

Happy Hacking,
Parisa, on behalf of Chrome Security

## Q2 2013

Hello from the Chrome Security Team!

For those that don’t know us, we’re here to help make Chrome a very (the most!)
[secure browser](http://www.chromium.org/Home/chromium-security). That boils
down to a fair amount of work on security reviews (and other consulting), but
here’s some insight into some of the other things we were up to last quarter:

Bug Fixin’ and Code Reviews

At the start of the quarter, we initiated a Code 28 on security bugs to trim
back the fat backlog of open issues. With the help of dozens of engineers across
Chrome, we fixed over 100 security bugs in just over 4 weeks and brought the
count of Medium+ severity issues to single digits. (We’ve lapsed a bit in the
past week, but hopefully will recover once everyone returns from July vacation
:)

As of July 1st, [Clusterfuzz](http://goto/clusterfuzz) has helped us find and
fix
[822](https://code.google.com/p/chromium/issues/list?can=1&q=type%3Abug-security+ClusterFuzz+status%3AFixed+closed-before%3A2013%2F7%2F1&sort=-id+-security_severity+-secseverity+-owner+-modified&colspec=ID+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner&x=m&y=releaseblock&cells=tiles)
bugs! Last quarter, we added a [new
check](http://svnsearch.org/svnsearch/repos/BLINK/search?logMessage=ASSERT_WITH_SECURITY_IMPLICATION)
to identify out of bound memory accesses and bad casts
(ASSERT_WITH_SECURITY_IMPLICATION), which resulted in
~[72](https://code.google.com/p/chromium/issues/list?can=1&q=type%3Abug-security+%22ASSERTION+FAILED%22+status%3AFixed+opened-after%3A2013%2F1%2F1&sort=-security_severity+-secseverity+-owner+-modified&colspec=ID+Stars+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner&x=m&y=releaseblock&cells=tiles)
new bugs identified and fixed. We’re also beta testing a “Fuzzer Donation”
extension to our [vulnerability reward
program](/Home/chromium-security/vulnerability-rewards-program).

Anecdotally, this quarter we noticed an increase in the number of IPC reviews
and marked decrease in security issues! Not sure if our recent [security tips
doc](/Home/chromium-security/security-tips-for-ipc) is to credit, but well done
to all the IPC authors and editors!

Process hardening

We’ve mostly wrapped up the [binding
integrity](/Home/chromium-security/binding-integrity) exploit mitigation changes
we started last quarter, and it’s now landed on all desktop platforms and Clank.
Remaining work entails [making additional V8 wrapped types inherit from
ScriptWrappable](https://code.google.com/p/chromium/issues/detail?id=236671&can=1&q=tsepez%20scriptwrappable&colspec=ID%20Pri%20M%20Iteration%20ReleaseBlock%20Cr%20Status%20Owner%20Summary%20OS%20Modified)
so more Chrome code benefits from this protection. We also started a new memory
hardening change that aims to [place DOM nodes inside their own heap
partition](https://code.google.com/p/chromium/issues/detail?id=246860). Why
would we want to do that? Used-after-free memory bugs are common. By having a
separate partition, the attacker gets a more limited choice of what to overlap
on top of the freed memory slot, which makes these types of bugs substantially
harder to exploit. (It turns out there is some performance improvement in doing
this too!)

Sandboxing++

We’re constantly trying to improve [Chrome
sandboxing](/developers/design-documents/sandbox). On [Chrome OS and
Linux](https://code.google.com/p/chromium/wiki/LinuxSandboxing), The GPU process
is now sandboxed on ARM
([M28](https://code.google.com/p/chromium/issues/detail?id=235870)) and we’ve
been been working on [sandboxing
NaCl](https://code.google.com/p/chromium/issues/detail?id=168812) under
[seccomp-bpf](http://blog.chromium.org/2012/11/a-safer-playground-for-your-linux-and.html).
We’ve also increased seccomp-bpf test coverage and locked down sandbox
parameters (i.e. less attack surface). Part of the Chrome seccomp-bpf sandbox is
now used in google3 (//third_party/chrome_seccomp), and Seccomp-legacy and
SELinux [have been
deprecated](https://code.google.com/p/chromium/wiki/LinuxSandboxing) as
sandboxing mechanisms.

Chrome work across platforms

    Mobile platforms pose a number of challenges to replicating some of the
    security features we’re most proud of on desktop, but with only expected
    growth of mobile, we know we need to shift some security love here. We’re
    getting more people ramped up to help on consulting (security and code
    reviews) and making headway on short and long-term goals.

    On Windows, we’re still chugging along sorting out tests and build
    infrastructure to get a stable Win64 release build for canary tests.

    On Chrome OS, work on kernel ASLR is ongoing, and we continued sandboxing
    [system
    daemons](https://code.google.com/p/chromium/issues/detail?id=224082).

Site Isolation Efforts

After some design and planning in Q1, we started building the early support for
[out-of-process
iframes](http://www.chromium.org/developers/design-documents/oop-iframes) so
that [Chrome's sandbox can help us enforce the Same Origin
Policy](http://www.chromium.org/developers/design-documents/site-isolation). In
Q2, we added a FrameTreeNode class to track frames in the browser process,
refactored some navigation logic, made DOMWindow own its Document (rather than
vice versa) in Blink, and got our prototype to handle simple input events. We'll
be using these changes to get basic out-of-process iframes working behind a flag
in Q3!

Extensions & Apps

This quarter, we detected and removed ~N bad extensions from the Web Store that
were either automatically detected or manually flagged as malicious or violating
our policies. We’ve started transitioning manual CRX malware reviews to a newly
formed team, who are staffing and ramping up to handle this significant
workload. Finally, we’ve been looking at ways to improve the permission dialog
for extensions so that it’s easier for users to understand the security
implications of what they’re installing, and working on a set of experiments to
understand how changes to the permissions dialog affect user installation of
malware.

Happy Q3!

Parisa, on behalf of Chrome Security

## Q1 2013

Hi from the Chrome Security Team!

For those that don’t know us already, we’re here to help make Chrome the [most
secure browser in the market](http://www.chromium.org/Home/chromium-security).
We do a fair bit of work on security reviews of new features (and other
consulting), but here’s a summary of some of the other things we were up to last
quarter:

Bug, bugs, bugs

Though some time is still spent [handeling external security
reports](http://www.chromium.org/Home/chromium-security/security-sheriff)
(mainly from participants of our [vulnerability reward
program](http://www.chromium.org/Home/chromium-security/vulnerability-rewards-program)),
we spent comparatively more time in Q1 hunting for security bugs ourselves. In
particular, we audited a bunch of IPC implementations after the
[two](http://blog.chromium.org/2012/10/pwnium-2-results-and-wrap-up_10.html)
[impressive](http://blog.chromium.org/2012/05/tale-of-two-pwnies-part-1.html)
IPC-based exploits from last year - aedla found some juicy sandbox bypass
vulnerabilities ([161564](http://crbug.com/161564),
[162114](http://crbug.com/162114), [167840](http://crbug.com/167840),
[169685](http://crbug.com/169685)) and cdn and cevans found / fixed a bunch of
other interesting memory corruption bugs
([169973](https://code.google.com/p/chromium/issues/detail?id=169973),
[166708](https://code.google.com/p/chromium/issues/detail?id=166708),
[164682](https://code.google.com/p/chromium/issues/detail?id=164682)).
Underground rumors indicate many of these internally discovered bugs collided
with discoveries from third party researchers (that were either sitting on or
using them for their own purposes). At this point, most of the IPCs that handle
file paths have been audited, and we’ve started putting together a doc with
[security tips to mind when writing
IPC](https://sites.google.com/a/google.com/chrome-security/security-tips-for-ipc).

On the fuzzing front, we updated and added a number of [fuzzers to
Clusterfuzz](https://cluster-fuzz.appspot.com/#fuzzers): HTML (ifratric,
mjurczyk), Flash (fjserna), CSS (bcrane), V8 (farcasia), Video VTT (yihongg),
extension APIs (meacer), WebRTC (phoglund), Canvas/Skia (aarya), and
Flicker/media (aarya); aarya also taught Clusterfuzz to look for dangerous
ASSERTs with security implications, which resulted in even more bugs. Kudos to
Clusterfuzz and the ASAN team for kicking out another [132 security
bugs](https://code.google.com/p/chromium/issues/list?can=1&q=type%3Abug-security+label%3AClusterFuzz+opened-after%3A2012%2F12%2F31+-status%3Awontfix+-status%3Aduplicate+opened-before%3A2013%2F4%2F1&sort=-id+-security_severity+-secseverity+-owner+-modified&colspec=ID+Stars+Pri+Status+Summary+Modified+OS+M+Security_severity+Security_impact+Owner&x=m&y=releaseblock&cells=tiles)
last quarter! One downside to all these new bugs is that our queue of open
security bugs across Chrome has really spiked ([85+ as of
today](https://code.google.com/p/chromium/issues/list?can=2&q=Security_Severity%3DCritical+OR+Security_Severity%3DHigh+OR+Security_Severity%3DMedium&colspec=ID+Pri+M+Iteration+ReleaseBlock+Cr+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=tiles)).
PIease help us fix these bugs!

Process hardening

We’re constantly thinking about proactive hardening we can add to Chrome to
eliminate or mitigate exploitation techniques. We find inspiration not only from
cutting edge security defense research, but also industry chatter around what
the grey and black hats are using to exploit Chrome and other browsers. This
past quarter jln implemented more fine grained support for [sandboxing on
Linux](https://code.google.com/p/chromium/wiki/LinuxSandboxing), in addition to
some low level tcmalloc changes that improve ASLR and general allocator security
on 64-bit platforms. With jorgelo, they also implemented support for a stronger
GPU sandbox on Chrome OS (which we believe was instrumental in avoiding a Pwnium
3 exploit). tsepez landed support for [V8 bindings
integrity](/Home/chromium-security/binding-integrity) on Linux and Mac OS, a
novel feature that ensures DOM objects are valid when bound to Javascript; this
avoids exploitation of type confusion bugs in the DOM, which Chrome has suffered
from in the past. palmer just enabled bindings integrity for Chrome on Android,
and work is in progress on Windows.

Work across platforms

One of our key goals is to get Chrome running natively on 64-bit Windows, where
the platform mitigations against certain attacks (such as heap spray) are
stronger than when running within a WOW64 process. (We’ve also seen some
performance bump on graphics and media on 64-bit Windows!) We made serious
progress on this work in Q1, coordinating with engineers on a dozen different
teams to land fixes in our codebase (and dependencies), working with Adobe on
early Flapper builds, porting components of the Windows sandbox to Win64, and
landing 100+ generic Win64 build system and API fixes. Thanks to all that have
made this possible!

As Chrome usage on mobile platforms increases, so too must our security
attention. We’ve set out some short and long-term goals for mobile Chrome
security, and are excited to start working with the Clank team on better
sandboxing and improved HTTPS authentication.

Site isolation

Work continues on the ambitious project to support [site-per-process
sandboxing](http://www.chromium.org/developers/design-documents/site-isolation),
which should help us [prevent additional attacks aimed at stealing or tampering
with user data from a specific
site](https://docs.google.com/a/google.com/document/d/1X5xZ2hYZurR_c2zU11AoEn15Ebu1er4cCLEudLJvPHA/edit).
Last quarter, we published a more complete [design for out-of-process
iframes](http://www.chromium.org/developers/design-documents/oop-iframes), set
up performance and testing infrastructure, and hacked together a prototype
implementation that helped confirm the feasibility of this project and surface
some challenges and open questions that need more investigation.

Extensions

When not feeding the team fish, meacer added a lot of features to Navitron to
make flagged extensions easier to review and remove from the WebStore. To put
this work in perspective, each week ~X new items are submitted to Webstore, ~Y
of them are automatically flagged as malware (and taken down), ~Z malware
escalations are manually escalated from extension reviewers (and then reviewed
again by security;. meacer also added a fuzzer for extensions and apps APIs, and
has been fixing [the resulting
bugs](https://code.google.com/p/chromium/issues/list?can=1&q=Fuzzer%3A+Meacer_extension_apis&colspec=ID+Pri+M+Iteration+ReleaseBlock+Cr+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=tiles).

Until we meet again (probably in the issue tracker)...

Parisa, on behalf of Chrome Security
