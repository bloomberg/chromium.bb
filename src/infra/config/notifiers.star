# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/branches.star", "branches")

luci.notifier(
    name = "chromesec-lkgr-failures",
    on_status_change = True,
    notify_emails = [
        "chromesec-lkgr-failures@google.com",
    ],
)

luci.notifier(
    name = "chrome-lacros-engprod-alerts",
    on_status_change = True,
    notify_emails = [
        "chrome-lacros-engprod-alerts@google.com",
    ],
)

luci.notifier(
    name = "chrome-memory-safety",
    on_status_change = True,
    notify_emails = [
        "chrome-memory-safety+bots@google.com",
    ],
)

luci.notifier(
    name = "chrome-memory-sheriffs",
    on_status_change = True,
    notify_emails = [
        "chrome-memory-sheriffs+bots@google.com",
    ],
)

luci.notifier(
    name = "chromium-androidx-packager",
    on_new_status = ["FAILURE"],
    notify_emails = [
        "clank-build-core+androidxfailures@google.com",
        "clank-library-failures+androidx@google.com",
    ],
)

luci.notifier(
    name = "chromium-3pp-packager",
    on_new_status = ["FAILURE"],
    notify_emails = [
        "chromium-3pp-packager+failures@google.com",
        "clank-build-core+3ppfailures@google.com",
    ],
)

luci.notifier(
    name = "cr-fuchsia",
    on_status_change = True,
    notify_emails = [
        "chrome-fuchsia-gardener@grotations.appspotmail.com",
    ],
)

luci.notifier(
    name = "cronet",
    on_occurrence = ["FAILURE", "INFRA_FAILURE"],
    notify_emails = [
        "cronet-sheriff@grotations.appspotmail.com",
    ],
)

luci.notifier(
    name = "metadata-mapping",
    on_new_status = ["FAILURE"],
    notify_emails = ["chromium-component-mapping@google.com"],
)

luci.notifier(
    name = "weblayer-sheriff",
    on_new_status = ["FAILURE"],
    notify_emails = [
        "weblayer-sheriff@grotations.appspotmail.com",
    ],
)

TREE_CLOSING_STEPS_REGEXP = "\\b({})\\b".format("|".join([
    "bot_update",
    "compile",
    "gclient runhooks",
    "runhooks",
    "update",
    "\\w*nocompile_test",
]))

# This results in a notifier with no recipients, so nothing will actually be
# notified. This still creates a "notifiable" that can be passed to the notifies
# argument of a builder, so conditional logic doesn't need to be used when
# setting the argument and erroneous tree closure notifications won't be sent
# for failures on branches.
def _empty_notifier(*, name):
    luci.notifier(
        name = name,
        on_new_status = ["INFRA_FAILURE"],
    )

def tree_closer(*, name, tree_status_host, **kwargs):
    if branches.matches(branches.MAIN):
        luci.tree_closer(
            name = name,
            tree_status_host = tree_status_host,
            **kwargs
        )
    else:
        _empty_notifier(name = name)

tree_closer(
    name = "chromium-tree-closer",
    tree_status_host = "chromium-status.appspot.com",
    failed_step_regexp = TREE_CLOSING_STEPS_REGEXP,
)

tree_closer(
    name = "close-on-any-step-failure",
    tree_status_host = "chromium-status.appspot.com",
)

def tree_closure_notifier(*, name, **kwargs):
    if branches.matches(branches.MAIN):
        luci.notifier(
            name = name,
            on_occurrence = ["FAILURE"],
            failed_step_regexp = TREE_CLOSING_STEPS_REGEXP,
            **kwargs
        )
    else:
        _empty_notifier(name = name)

tree_closure_notifier(
    name = "chromium-tree-closer-email",
    notify_rotation_urls = [
        "https://chrome-ops-rotation-proxy.appspot.com/current/oncallator:chrome-build-sheriff",
    ],
    template = luci.notifier_template(
        name = "tree_closure_email_template",
        body = io.read_file("templates/tree_closure_email.template"),
    ),
)

tree_closure_notifier(
    name = "gpu-tree-closer-email",
    notify_emails = ["chrome-gpu-build-failures@google.com"],
    notify_rotation_urls = [
        "https://chrome-ops-rotation-proxy.appspot.com/current/oncallator:chrome-gpu-pixel-wrangler",
    ],
)

tree_closure_notifier(
    name = "linux-memory",
    notify_emails = ["thomasanderson@chromium.org"],
)

tree_closure_notifier(
    name = "linux-archive-rel",
    notify_emails = ["thomasanderson@chromium.org"],
)

tree_closure_notifier(
    name = "Deterministic Android",
    notify_emails = ["agrieve@chromium.org"],
)

tree_closure_notifier(
    name = "Deterministic Linux",
    notify_emails = [
        "tikuta@chromium.org",
        "ukai@chromium.org",
        "yyanagisawa@chromium.org",
    ],
)

tree_closure_notifier(
    name = "linux-ozone-rel",
    notify_emails = [
        "fwang@chromium.org",
        "maksim.sisov@chromium.org",
        "rjkroege@chromium.org",
        "thomasanderson@chromium.org",
        "timbrown@chromium.org",
        "tonikitoo@chromium.org",
    ],
)

luci.notifier(
    name = "Site Isolation Android",
    notify_emails = [
        "nasko+fyi-bots@chromium.org",
        "creis+fyi-bots@chromium.org",
        "lukasza+fyi-bots@chromium.org",
        "alexmos+fyi-bots@chromium.org",
    ],
    on_new_status = ["FAILURE"],
)

luci.notifier(
    name = "CFI Linux",
    notify_emails = [
        "pcc@chromium.org",
    ],
    on_new_status = ["FAILURE"],
)

luci.notifier(
    name = "Win 10 Fast Ring",
    notify_emails = [
        "wfh@chromium.org",
    ],
    on_new_status = ["FAILURE"],
)

luci.notifier(
    name = "linux-blink-fyi-bots",
    notify_emails = [
        "mlippautz+fyi-bots@chromium.org",
    ],
    on_new_status = ["FAILURE"],
)

luci.notifier(
    name = "annotator-rel",
    notify_emails = [
        "pastarmovj@chromium.org",
        "nicolaso@chromium.org",
    ],
    on_new_status = ["FAILURE"],
)

tree_closure_notifier(
    name = "chromium.linux",
    notify_emails = [
        "thomasanderson@chromium.org",
    ],
)
