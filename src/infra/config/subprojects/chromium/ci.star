load('//lib/builders.star', 'builder_name', 'cpu', 'goma', 'os')
load('//lib/ci.star', 'ci')
load('//project.star', 'settings')


ci.set_defaults(
    settings,
    add_to_console_view = True,
    bucketed_triggers = settings.is_master,
    main_console_view = settings.main_console_name,
)

ci.declare_bucket(settings)


# Automatically maintained consoles

ci.console_view(
    name = 'chromium',
    include_experimental_builds = True,
    ordering = {},
)

ci.console_view(
    name = 'chromium.android',
    ordering = {
        None: ['cronet', 'builder', 'tester'],
        '*cpu*': ['arm', 'arm64', 'x86'],
        'cronet': '*cpu*',
        'builder': '*cpu*',
        'builder|det': ci.ordering(short_names=['rel', 'dbg']),
        'tester': ['phone', 'tablet'],
        'builder_tester|arm64': ci.ordering(short_names=['M proguard']),
    },
)

ci.console_view(
    name = 'chromium.android.fyi',
    ordering = {
        None: ['android', 'memory', 'weblayer', 'webview'],
    },
)

ci.console_view(
    name = 'chromium.chromiumos',
    ordering = {
        None: ['default'],
        'default': ci.ordering(short_names=['ful', 'rel']),
        'simple': ['release', 'debug'],
    },
)

ci.console_view(
    name = 'chromium.clang',
    ordering = {
        None: [
            'ToT Linux',
            'ToT Android',
            'ToT Mac',
            'ToT Windows',
            'ToT Code Coverage',
        ],
        'ToT Linux': ci.ordering(
            short_names=['rel', 'ofi', 'dbg', 'asn', 'fuz', 'msn', 'tsn'],
        ),
        'ToT Android': ci.ordering(short_names=['rel', 'dbg', 'x64']),
        'ToT Mac': ci.ordering(short_names=['rel', 'ofi', 'dbg']),
        'ToT Windows': ci.ordering(
            short_names=['rel', 'ofi'],
            categories=['x64'],
        ),
        'ToT Windows|x64': ci.ordering(short_names=['rel']),
        'CFI|Win': ci.ordering(short_names=['x86', 'x64']),
        'iOS': ['public'],
        'iOS|public': ci.ordering(short_names=['sim', 'dev']),
    },
)

ci.console_view(
    name = 'chromium.dawn',
    ordering = {
        None: ['ToT'],
        '*builder*': ['Builder'],
        '*cpu*': ci.ordering(short_names=['x86']),
        'ToT|Mac': '*builder*',
        'ToT|Windows|Builder': '*cpu*',
        'ToT|Windows|Intel': '*cpu*',
        'ToT|Windows|Nvidia': '*cpu*',
        'DEPS|Mac': '*builder*',
        'DEPS|Windows|Builder': '*cpu*',
        'DEPS|Windows|Intel': '*cpu*',
        'DEPS|Windows|Nvidia': '*cpu*',
    },
)

ci.console_view(
    name = 'chromium.fyi',
    ordering = {
        None: [
            'closure_compilation',
            'code_coverage',
            'cronet',
            'mac',
            'deterministic',
            'fuchsia',
            'chromeos',
            'iOS',
            'linux',
            'mojo',
            'recipe',
            'remote_run',
            'site_isolation',
            'network',
            'viz',
            'win10',
            'win32',
        ],
        'code_coverage': ci.ordering(
            short_names=['and', 'ann', 'lnx', 'lcr', 'mac']
        ),
        'mac': ci.ordering(short_names=['bld', '15', 'herm']),
        'deterministic|mac': ci.ordering(short_names=['rel', 'dbg']),
        'iOS|iOS13': ci.ordering(short_names=['dev', 'sim']),
        'linux|blink': ci.ordering(short_names=['TD']),
    },
)

ci.console_view(
    name ='chromium.fuzz',
    ordering = {
        None: [
            'afl',
            'win asan',
            'mac asan',
            'cros asan',
            'linux asan',
            'libfuzz',
            'linux msan',
            'linux tsan',
        ],
        '*config*': ci.ordering(short_names=['dbg', 'rel']),
        'win asan': '*config*',
        'mac asan': '*config*',
        'linux asan': '*config*',
        'linux asan|x64 v8-ARM': '*config*',
        'libfuzz': ci.ordering(short_names=[
            'chromeos-asan',
            'linux32',
            'linux32-dbg',
            'linux',
            'linux-dbg',
            'linux-msan',
            'linux-ubsan',
            'mac-asan',
            'win-asan',
        ]),
    },
)

ci.console_view(
    name = 'chromium.gpu',
    ordering = {
        None: ['Windows', 'Mac', 'Linux'],
    },
)

ci.console_view(
    name = 'chromium.gpu.fyi',
    ordering = {
        None: ['Windows', 'Mac', 'Linux'],
        '*builder*': ['Builder'],
        '*type*': ci.ordering(short_names=['rel', 'dbg', 'exp']),
        '*cpu*': ci.ordering(short_names=['x86']),
        'Windows': '*builder*',
        'Windows|Builder': ['Release', 'dEQP', 'dx12vk', 'Debug'],
        'Windows|Builder|Release': '*cpu*',
        'Windows|Builder|dEQP': '*cpu*',
        'Windows|Builder|dx12vk': '*type*',
        'Windows|Builder|Debug': '*cpu*',
        'Windows|10|x64|Intel': '*type*',
        'Windows|10|x64|Nvidia': '*type*',
        'Windows|10|x86|Nvidia': '*type*',
        'Windows|7|x64|Nvidia': '*type*',
        'Mac': '*builder*',
        'Mac|Builder': '*type*',
        'Mac|AMD|Retina': '*type*',
        'Mac|Intel': '*type*',
        'Mac|Nvidia': '*type*',
        'Linux': '*builder*',
        'Linux|Builder': '*type*',
        'Linux|Intel': '*type*',
        'Linux|Nvidia': '*type*',
        'Android': ['L32', 'M64', 'N64', 'P32', 'vk', 'dqp', 'skgl', 'skv'],
        'Android|M64': ['QCOM'],
    },
)

ci.console_view(
    name = 'chromium.linux',
    ordering = {
        None: ['release', 'debug'],
        'release': ci.ordering(short_names=['bld', 'tst', 'nsl', 'gcc']),
        'cast': ci.ordering(short_names=['vid', 'aud']),
    },
)

ci.console_view(
    name = 'chromium.mac',
    ordering = {
        None: ['release'],
        'release': ci.ordering(short_names=['bld']),
        'debug': ci.ordering(short_names=['bld']),
        'ios|default': ci.ordering(short_names=['dev', 'sim']),
    },
)

ci.console_view(
    name = 'chromium.memory',
    ordering = {
        None: ['win', 'mac', 'linux', 'cros'],
        '*build-or-test*': ci.ordering(short_names=['bld', 'tst']),
        'linux|TSan v2': '*build-or-test*',
        'linux|asan lsan': '*build-or-test*',
        'linux|webkit': ci.ordering(short_names=['asn', 'msn']),
    },
)

ci.console_view(
    name = 'chromium.swangle',
    ordering = {
        None: ['DEPS', 'ToT ANGLE', 'ToT SwiftShader'],
        '*os*': ['Windows', 'Mac'],
        '*cpu*': ci.ordering(short_names=['x86', 'x64']),
        'DEPS': '*os*',
        'DEPS|Windows': '*cpu*',
        'DEPS|Linux': '*cpu*',
        'ToT ANGLE': '*os*',
        'ToT ANGLE|Windows': '*cpu*',
        'ToT ANGLE|Linux': '*cpu*',
        'ToT SwiftShader': '*os*',
        'ToT SwiftShader|Windows': '*cpu*',
        'ToT SwiftShader|Linux': '*cpu*',
        'Chromium': '*os*',
    },
)

ci.console_view(
    name = 'chromium.win',
    ordering = {
        None: ['release', 'debug'],
        'debug|builder': ci.ordering(short_names=['64', '32']),
        'debug|tester': ci.ordering(short_names=['7', '10']),
    },
)

# Builders are sorted first lexicographically by the function used to define
# them, then lexicographically by their name


ci.android_builder(
    name = 'Android WebView M (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'tester|webview',
        short_name = 'M',
    ),
    triggered_by = [builder_name('Android arm64 Builder (dbg)')],
)

ci.android_builder(
    name = 'Android WebView N (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'tester|webview',
        short_name = 'N',
    ),
    triggered_by = [builder_name('Android arm64 Builder (dbg)')],
)

ci.android_builder(
    name = 'Android WebView O (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'tester|webview',
        short_name = 'O',
    ),
    triggered_by = [builder_name('Android arm64 Builder (dbg)')],
)

ci.android_builder(
    name = 'Android WebView P (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'tester|webview',
        short_name = 'P',
    ),
    triggered_by = [builder_name('Android arm64 Builder (dbg)')],
)

ci.android_builder(
    name = 'Android arm Builder (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'builder|arm',
        short_name = '32',
    ),
    execution_timeout = 4 * time.hour,
)

ci.android_builder(
    name = 'Android arm64 Builder (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'builder|arm',
        short_name = '64',
    ),
    goma_jobs = goma.jobs.MANY_JOBS_FOR_CI,
    execution_timeout = 4 * time.hour,
)

ci.android_builder(
    name = 'Android x64 Builder (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'builder|x86',
        short_name = '64',
    ),
    execution_timeout = 4 * time.hour,
)

ci.android_builder(
    name = 'Android x86 Builder (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'builder|x86',
        short_name = '32',
    ),
)

ci.android_builder(
    name = 'Cast Android (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'on_cq',
        short_name = 'cst',
    ),
)

ci.android_builder(
    name = 'Marshmallow 64 bit Tester',
    console_view_entry = ci.console_view_entry(
        category = 'tester|phone',
        short_name = 'M',
    ),
    triggered_by = [builder_name('Android arm64 Builder (dbg)')],
)

ci.android_builder(
    name = 'Nougat Phone Tester',
    console_view_entry = ci.console_view_entry(
        category = 'tester|phone',
        short_name = 'N',
    ),
    triggered_by = [builder_name('Android arm64 Builder (dbg)')],
)

ci.android_builder(
    name = 'Oreo Phone Tester',
    console_view_entry = ci.console_view_entry(
        category = 'tester|phone',
        short_name = 'O',
    ),
    triggered_by = [builder_name('Android arm64 Builder (dbg)')],
)

ci.android_builder(
    name = 'android-cronet-arm-dbg',
    console_view_entry = ci.console_view_entry(
        category = 'cronet|arm',
        short_name = 'dbg',
    ),
    notifies = ['cronet'],
)

ci.android_builder(
    name = 'android-cronet-arm-rel',
    console_view_entry = ci.console_view_entry(
        category = 'cronet|arm',
        short_name = 'rel',
    ),
    notifies = ['cronet'],
)

ci.android_builder(
    name = 'android-cronet-kitkat-arm-rel',
    console_view_entry = ci.console_view_entry(
        category = 'cronet|test',
        short_name = 'k',
    ),
    notifies = ['cronet'],
    triggered_by = [builder_name('android-cronet-arm-rel')],
)

ci.android_builder(
    name = 'android-cronet-lollipop-arm-rel',
    console_view_entry = ci.console_view_entry(
        category = 'cronet|test',
        short_name = 'l',
    ),
    notifies = ['cronet'],
    triggered_by = [builder_name('android-cronet-arm-rel')],
)

ci.android_builder(
    name = 'android-lollipop-arm-rel',
    console_view_entry = ci.console_view_entry(
        category = 'on_cq',
        short_name = 'L',
    ),
)

ci.android_builder(
    name = 'android-marshmallow-arm64-rel',
    console_view_entry = ci.console_view_entry(
        category = 'on_cq',
        short_name = 'M',
    ),
)

ci.android_builder(
    name = 'android-pie-arm64-dbg',
    console_view_entry = ci.console_view_entry(
        category = 'tester|phone',
        short_name = 'P',
    ),
    triggered_by = [builder_name('Android arm64 Builder (dbg)')],
)

ci.android_builder(
    name = 'android-pie-arm64-rel',
    console_view_entry = ci.console_view_entry(
        category = 'on_cq',
        short_name = 'P',
    ),
)


ci.chromiumos_builder(
    name = 'chromeos-amd64-generic-dbg',
    console_view_entry = ci.console_view_entry(
        category = 'simple|debug|x64',
        short_name = 'dbg',
    ),
)

ci.chromiumos_builder(
    name = 'chromeos-amd64-generic-rel',
    console_view_entry = ci.console_view_entry(
        category = 'simple|release|x64',
        short_name = 'rel',
    ),
)

ci.chromiumos_builder(
    name = 'chromeos-arm-generic-rel',
    console_view_entry = ci.console_view_entry(
        category = 'simple|release',
        short_name = 'arm',
    ),
)

ci.chromiumos_builder(
    name = 'chromeos-kevin-rel',
    console_view_entry = ci.console_view_entry(
        category = 'simple|release',
        short_name = 'kvn',
    ),
)

ci.fyi_builder(
    name = 'chromeos-kevin-rel-hw-tests',
    console_view_entry = ci.console_view_entry(
        category = 'chromeos',
    ),
)

ci.chromiumos_builder(
    name = 'linux-chromeos-dbg',
    console_view_entry = ci.console_view_entry(
        category = 'default',
        short_name = 'dbg',
    ),
)

ci.chromiumos_builder(
    name = 'linux-chromeos-rel',
    console_view_entry = ci.console_view_entry(
        category = 'default',
        short_name = 'rel',
    ),
)


ci.dawn_builder(
    name = 'Dawn Linux x64 DEPS Builder',
    console_view_entry = ci.console_view_entry(
        category = 'DEPS|Linux|Builder',
        short_name = 'x64',
    ),
)

ci.dawn_builder(
    name = 'Dawn Linux x64 DEPS Release (Intel HD 630)',
    console_view_entry = ci.console_view_entry(
        category = 'DEPS|Linux|Intel',
        short_name = 'x64',
    ),
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name('Dawn Linux x64 DEPS Builder')],
)

ci.dawn_builder(
    name = 'Dawn Linux x64 DEPS Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'DEPS|Linux|Nvidia',
        short_name = 'x64',
    ),
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name('Dawn Linux x64 DEPS Builder')],
)

ci.dawn_builder(
    name = 'Dawn Mac x64 DEPS Builder',
    builderless = False,
    console_view_entry = ci.console_view_entry(
        category = 'DEPS|Mac|Builder',
        short_name = 'x64',
    ),
    cores = None,
    os = os.MAC_ANY,
)

# Note that the Mac testers are all thin Linux VMs, triggering jobs on the
# physical Mac hardware in the Swarming pool which is why they run on linux
ci.dawn_builder(
    name = 'Dawn Mac x64 DEPS Release (AMD)',
    console_view_entry = ci.console_view_entry(
        category = 'DEPS|Mac|AMD',
        short_name = 'x64',
    ),
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name('Dawn Mac x64 DEPS Builder')],
)

ci.dawn_builder(
    name = 'Dawn Mac x64 DEPS Release (Intel)',
    console_view_entry = ci.console_view_entry(
        category = 'DEPS|Mac|Intel',
        short_name = 'x64',
    ),
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name('Dawn Mac x64 DEPS Builder')],
)

ci.dawn_builder(
    name = 'Dawn Win10 x64 DEPS Builder',
    console_view_entry = ci.console_view_entry(
        category = 'DEPS|Windows|Builder',
        short_name = 'x64',
    ),
    os = os.WINDOWS_ANY,
)

ci.dawn_builder(
    name = 'Dawn Win10 x64 DEPS Release (Intel HD 630)',
    console_view_entry = ci.console_view_entry(
        category = 'DEPS|Windows|Intel',
        short_name = 'x64',
    ),
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name('Dawn Win10 x64 DEPS Builder')],
)

ci.dawn_builder(
    name = 'Dawn Win10 x64 DEPS Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'DEPS|Windows|Nvidia',
        short_name = 'x64',
    ),
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name('Dawn Win10 x64 DEPS Builder')],
)

ci.dawn_builder(
    name = 'Dawn Win10 x86 DEPS Builder',
    console_view_entry = ci.console_view_entry(
        category = 'DEPS|Windows|Builder',
        short_name = 'x86',
    ),
    os = os.WINDOWS_ANY,
)

ci.dawn_builder(
    name = 'Dawn Win10 x86 DEPS Release (Intel HD 630)',
    console_view_entry = ci.console_view_entry(
        category = 'DEPS|Windows|Intel',
        short_name = 'x86',
    ),
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name('Dawn Win10 x86 DEPS Builder')],
)

ci.dawn_builder(
    name = 'Dawn Win10 x86 DEPS Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'DEPS|Windows|Nvidia',
        short_name = 'x86',
    ),
    cores = 2,
    os = os.LINUX_DEFAULT,
    triggered_by = [builder_name('Dawn Win10 x86 DEPS Builder')],
)


ci.fyi_builder(
    name = 'VR Linux',
    console_view_entry = ci.console_view_entry(
        category = 'linux',
    ),
)

# This is launching & collecting entirely isolated tests.
# OS shouldn't matter.
ci.fyi_builder(
    name = 'mac-osxbeta-rel',
    console_view_entry = ci.console_view_entry(
        category = 'mac',
        short_name = 'beta',
    ),
    goma_backend = None,
    triggered_by = [builder_name('Mac Builder')],
)


ci.fyi_ios_builder(
    name = 'ios-simulator-cronet',
    console_view_entry = ci.console_view_entry(
        category = 'cronet',
    ),
    executable = 'recipe:chromium',
    notifies = ['cronet'],
    properties = {
        'xcode_build_version': '11c29',
    },
)


ci.fyi_windows_builder(
    name = 'Win10 Tests x64 1803',
    console_view_entry = ci.console_view_entry(
        category = 'win10|1803',
    ),
    goma_backend = None,
    os = os.WINDOWS_10,
    triggered_by = [builder_name('Win x64 Builder')],
)


ci.gpu_builder(
    name = 'Android Release (Nexus 5X)',
    console_view_entry = ci.console_view_entry(
        category = 'Android',
    ),
)

ci.gpu_builder(
    name = 'GPU Linux Builder',
    console_view_entry = ci.console_view_entry(
        category = 'Linux',
    ),
)

ci.gpu_builder(
    name = 'GPU Mac Builder',
    console_view_entry = ci.console_view_entry(
        category = 'Mac',
    ),
    cores = None,
    os = os.MAC_ANY,
)

ci.gpu_builder(
    name = 'GPU Win x64 Builder',
    builderless = True,
    console_view_entry = ci.console_view_entry(
        category = 'Windows',
    ),
    os = os.WINDOWS_ANY,
)


ci.gpu_thin_tester(
    name = 'Linux Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Linux',
    ),
    triggered_by = [builder_name('GPU Linux Builder')],
)

ci.gpu_thin_tester(
    name = 'Mac Release (Intel)',
    console_view_entry = ci.console_view_entry(
        category = 'Mac',
    ),
    triggered_by = [builder_name('GPU Mac Builder')],
)

ci.gpu_thin_tester(
    name = 'Mac Retina Release (AMD)',
    console_view_entry = ci.console_view_entry(
        category = 'Mac',
    ),
    triggered_by = [builder_name('GPU Mac Builder')],
)

ci.gpu_thin_tester(
    name = 'Win10 x64 Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows',
    ),
    triggered_by = [builder_name('GPU Win x64 Builder')],
)


ci.linux_builder(
    name = 'Cast Linux',
    console_view_entry = ci.console_view_entry(
        category = 'cast',
        short_name = 'vid',
    ),
    goma_jobs = goma.jobs.J50,
)

ci.linux_builder(
    name = 'Fuchsia ARM64',
    console_view_entry = ci.console_view_entry(
        category = 'fuchsia|a64',
        short_name = 'rel',
    ),
    notifies = ['cr-fuchsia'],
)

ci.linux_builder(
    name = 'Fuchsia x64',
    console_view_entry = ci.console_view_entry(
        category = 'fuchsia|x64',
        short_name = 'rel',
    ),
    notifies = ['cr-fuchsia'],
)

ci.linux_builder(
    name = 'Linux Builder',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = 'bld',
    ),
)

ci.linux_builder(
    name = 'Linux Builder (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'debug|builder',
        short_name = '64',
    ),
)

ci.linux_builder(
    name = 'Linux Tests',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = 'tst',
    ),
    goma_backend = None,
    triggered_by = [builder_name('Linux Builder')],
)

ci.linux_builder(
    name = 'Linux Tests (dbg)(1)',
    console_view_entry = ci.console_view_entry(
        category = 'debug|tester',
        short_name = '64',
    ),
    triggered_by = [builder_name('Linux Builder (dbg)')],
)

ci.linux_builder(
    name = 'fuchsia-arm64-cast',
    console_view_entry = ci.console_view_entry(
        category = 'fuchsia|cast',
        short_name = 'a64',
    ),
    notifies = ['cr-fuchsia'],
)

ci.linux_builder(
    name = 'fuchsia-x64-cast',
    console_view_entry = ci.console_view_entry(
        category = 'fuchsia|cast',
        short_name = 'x64',
    ),
    notifies = ['cr-fuchsia'],
)

ci.linux_builder(
    name = 'linux-ozone-rel',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = 'ozo',
    ),
)

ci.linux_builder(
    name = 'Linux Ozone Tester (Headless)',
    console_view = 'chromium.fyi',
    console_view_entry = ci.console_view_entry(
        category = 'linux',
        short_name = 'loh',
    ),
    triggered_by = [builder_name('linux-ozone-rel')],
)

ci.linux_builder(
    name = 'Linux Ozone Tester (Wayland)',
    console_view = 'chromium.fyi',
    console_view_entry = ci.console_view_entry(
        category = 'linux',
        short_name = 'low',
    ),
    triggered_by = [builder_name('linux-ozone-rel')],
)

ci.linux_builder(
    name = 'Linux Ozone Tester (X11)',
    console_view = 'chromium.fyi',
    console_view_entry = ci.console_view_entry(
        category = 'linux',
        short_name = 'lox',
    ),
    triggered_by = [builder_name('linux-ozone-rel')],
)


ci.mac_builder(
    name = 'Mac Builder',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = 'bld',
    ),
    os = os.MAC_10_14,
)

ci.mac_builder(
    name = 'Mac Builder (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'debug',
        short_name = 'bld',
    ),
    os = os.MAC_ANY,
)

ci.thin_tester(
    name = 'Mac10.10 Tests',
    mastername = 'chromium.mac',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = '10',
    ),
    triggered_by = [builder_name('Mac Builder')],
)

ci.thin_tester(
    name = 'Mac10.11 Tests',
    mastername = 'chromium.mac',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = '11',
    ),
    triggered_by = [builder_name('Mac Builder')],
)

ci.thin_tester(
    name = 'Mac10.12 Tests',
    mastername = 'chromium.mac',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = '12',
    ),
    triggered_by = [builder_name('Mac Builder')],
)

ci.thin_tester(
    name = 'Mac10.13 Tests',
    mastername = 'chromium.mac',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = '13',
    ),
    triggered_by = [builder_name('Mac Builder')],
)

ci.thin_tester(
    name = 'Mac10.14 Tests',
    mastername = 'chromium.mac',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = '14',
    ),
    triggered_by = [builder_name('Mac Builder')],
)

ci.thin_tester(
    name = 'Mac10.13 Tests (dbg)',
    mastername = 'chromium.mac',
    console_view_entry = ci.console_view_entry(
        category = 'debug',
        short_name = '13',
    ),
    triggered_by = [builder_name('Mac Builder (dbg)')],
)

ci.thin_tester(
    name = 'WebKit Mac10.13 (retina)',
    mastername = 'chromium.mac',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = 'ret',
    ),
    triggered_by = [builder_name('Mac Builder')],
)


ci.mac_ios_builder(
    name = 'ios-simulator',
    console_view_entry = ci.console_view_entry(
        category = 'ios|default',
        short_name = 'sim',
    ),
    executable = 'recipe:chromium',
    properties = {
        'xcode_build_version': '11c29',
    },
)

ci.mac_ios_builder(
    name = 'ios-simulator-full-configs',
    console_view_entry = ci.console_view_entry(
        category = 'ios|default',
        short_name = 'ful',
    ),
    executable = 'recipe:chromium',
)


ci.memory_builder(
    name = 'Linux ASan LSan Builder',
    console_view_entry = ci.console_view_entry(
        category = 'linux|asan lsan',
        short_name = 'bld',
    ),
    ssd = True,
)

ci.memory_builder(
    name = 'Linux ASan LSan Tests (1)',
    console_view_entry = ci.console_view_entry(
        category = 'linux|asan lsan',
        short_name = 'tst',
    ),
    triggered_by = [builder_name('Linux ASan LSan Builder')],
)

ci.memory_builder(
    name = 'Linux ASan Tests (sandboxed)',
    console_view_entry = ci.console_view_entry(
        category = 'linux|asan lsan',
        short_name = 'sbx',
    ),
    triggered_by = [builder_name('Linux ASan LSan Builder')],
)

ci.memory_builder(
    name = 'Linux TSan Builder',
    console_view_entry = ci.console_view_entry(
        category = 'linux|TSan v2',
        short_name = 'bld',
    ),
)

ci.memory_builder(
    name = 'Linux TSan Tests',
    console_view_entry = ci.console_view_entry(
        category = 'linux|TSan v2',
        short_name = 'tst',
    ),
    triggered_by = [builder_name('Linux TSan Builder')],
)


ci.win_builder(
    name = 'Win7 Tests (dbg)(1)',
    console_view_entry = ci.console_view_entry(
        category = 'debug|tester',
        short_name = '7',
    ),
    os = os.WINDOWS_7,
    triggered_by = [builder_name('Win Builder (dbg)')],
)

ci.win_builder(
    name = 'Win 7 Tests x64 (1)',
    console_view_entry = ci.console_view_entry(
        category = 'release|tester',
        short_name = '64',
    ),
    os = os.WINDOWS_7,
    triggered_by = [builder_name('Win x64 Builder')],
)

ci.win_builder(
    name = 'Win Builder (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'debug|builder',
        short_name = '32',
    ),
    cores = 32,
    os = os.WINDOWS_ANY,
)

ci.win_builder(
    name = 'Win x64 Builder',
    console_view_entry = ci.console_view_entry(
        category = 'release|builder',
        short_name = '64',
    ),
    cores = 32,
    os = os.WINDOWS_ANY,
)

ci.win_builder(
    name = 'Win10 Tests x64',
    console_view_entry = ci.console_view_entry(
        category = 'release|tester',
        short_name = 'w10',
    ),
    triggered_by = [builder_name('Win x64 Builder')],
)
