exec('./ci.star')
exec('./gpu.try.star')
exec('./swangle.try.star')
exec('./try.star')

exec('./consoles/android.packager.star')
exec('./consoles/luci.chromium.try.star')
exec('./consoles/sheriff.ios.star')

exec('./fallback-cq.star')
