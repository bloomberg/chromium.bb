luci.console_view(
    name = 'chromium.dev',
    header = '//dev/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(builder = 'ci/android-lollipop-arm-rel-swarming'),
        luci.console_view_entry(builder = 'ci/android-marshmallow-arm64-rel-swarming'),
        luci.console_view_entry(builder = 'ci/linux-rel-swarming'),
        luci.console_view_entry(builder = 'ci/mac-rel-swarming'),
        luci.console_view_entry(builder = 'ci/win-rel-swarming'),
   ],
)

luci.console_view(
    name = 'chromium.staging',
    header = '//dev/chromium-header.textpb',
    repo = 'https://chromium.googlesource.com/chromium/src',
    entries = [
        luci.console_view_entry(builder = 'ci/linux-rel-swarming-staging'),
   ],
)
