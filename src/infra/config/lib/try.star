# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for defining try builders.

The `try_builder` function defined in this module enables defining a builder and
the tryjob verifier for it in the same location. It can also be accessed through
`try_.builder`.

The `tryjob` function specifies the verifier details for a builder. It can also
be accessed through `try_.job`.

The `defaults` struct provides module-level defaults for the arguments to
`try_builder`. The parameters that support module-level defaults have a
corresponding attribute on `defaults` that is a `lucicfg.var` that can be used
to set the default value. Can also be accessed through `try_.defaults`.
"""

load("./args.star", "args")
load("./branches.star", "branches")
load("./builders.star", "builder_url", "builders", "compilator_watcher_git_revision", "os", "os_category")
load("//project.star", "settings")

DEFAULT_EXCLUDE_REGEXPS = [
    # Contains documentation that doesn't affect the outputs
    ".+/[+]/docs/.+",
    # Contains configuration files that aren't active until after committed
    ".+/[+]/infra/config/.+",
]

defaults = args.defaults(
    extends = builders.defaults,
    check_for_flakiness = False,
    cq_group = None,
    main_list_view = None,
    subproject_list_view = None,
    resultdb_bigquery_exports = [],
)

def tryjob(
        *,
        disable_reuse = None,
        experiment_percentage = None,
        location_regexp = None,
        location_regexp_exclude = None,
        cancel_stale = None,
        add_default_excludes = True):
    """Specifies the details of a tryjob verifier.

    See https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/lucicfg/doc/README.md#luci.cq_tryjob_verifier
    for details on the most of the arguments.

    Arguments:
      add_default_excludes - A bool indicating whether to add exclude regexps
        for certain directories that would have no impact when building chromium
        with the patch applied (docs, config files that don't take effect until
        landing, etc., see DEFAULT_EXCLUDE_REGEXPS).

    Returns:
      A struct that can be passed to the `tryjob` argument of `try_.builder` to
      enable the builder for CQ.
    """
    return struct(
        disable_reuse = disable_reuse,
        experiment_percentage = experiment_percentage,
        add_default_excludes = add_default_excludes,
        location_regexp = location_regexp,
        location_regexp_exclude = location_regexp_exclude,
        cancel_stale = cancel_stale,
    )

def try_builder(
        *,
        name,
        branch_selector = branches.MAIN,
        check_for_flakiness = args.DEFAULT,
        cq_group = args.DEFAULT,
        list_view = args.DEFAULT,
        main_list_view = args.DEFAULT,
        subproject_list_view = args.DEFAULT,
        tryjob = None,
        experiments = None,
        resultdb_bigquery_exports = args.DEFAULT,
        **kwargs):
    """Define a try builder.

    Arguments:
      name - name of the builder, will show up in UIs and logs. Required.
      branch_selector - A branch selector value controlling whether the
        builder definition is executed. See branches.star for more
        information.
      check_for_flakiness - If True, it checks for new tests in a given try
        build and reruns them multiple times to ensure that they are not
        flaky.
      cq_group - The CQ group to add the builder to. If tryjob is None, it will
        be added as includable_only.
      list_view - A string or list of strings identifying the ID(s) of the list
        view to add an entry to. Supports a module-level default that defaults
        to the group of the builder, if provided.
      main_console_view - A string identifying the ID of the main list
        view to add an entry to. Supports a module-level default that
        defaults to None.
      subproject_list_view - A string identifying the ID of the
        subproject list view to add an entry to. Suppoers a module-level
        default that defaults to None.
      tryjob - A struct containing the details of the tryjob verifier for the
        builder, obtained by calling the `tryjob` function.
      experiments - a dict of experiment name to the percentage chance (0-100)
        that it will apply to builds generated from this builder.
      resultdb_bigquery_exports - a list of resultdb.export_test_results(...)
        specifying additional parameters for exporting test results to BigQuery.
        Will always upload to the following tables in addition to any tables
        specified by the list's elements:
          chrome-luci-data.chromium.try_test_results
          chrome-luci-data.gpu_try_test_results
    """
    if not branches.matches(branch_selector):
        return

    experiments = experiments or {}

    merged_resultdb_bigquery_exports = [
        resultdb.export_test_results(
            bq_table = "chrome-luci-data.chromium.try_test_results",
        ),
        resultdb.export_test_results(
            bq_table = "chrome-luci-data.chromium.gpu_try_test_results",
            predicate = resultdb.test_result_predicate(
                # Only match the telemetry_gpu_integration_test and
                # fuchsia_telemetry_gpu_integration_test targets.
                # Android Telemetry targets also have a suffix added to the end
                # denoting the binary that's included, so also catch those with
                # [^/]*.
                test_id_regexp = "ninja://(chrome/test:|content/test:fuchsia_)telemetry_gpu_integration_test[^/]*/.+",
            ),
        ),
        resultdb.export_test_results(
            bq_table = "chrome-luci-data.chromium.blink_web_tests_try_test_results",
            predicate = resultdb.test_result_predicate(
                # Match the "blink_web_tests" target and all of its
                # flag-specific versions, e.g. "vulkan_swiftshader_blink_web_tests".
                test_id_regexp = "ninja://[^/]*blink_web_tests/.+",
            ),
        ),
    ]
    merged_resultdb_bigquery_exports.extend(
        defaults.get_value(
            "resultdb_bigquery_exports",
            resultdb_bigquery_exports,
        ),
    )

    list_view = defaults.get_value("list_view", list_view)
    if list_view == args.COMPUTE:
        list_view = defaults.get_value_from_kwargs("builder_group", kwargs)
    if type(list_view) == type(""):
        list_view = [list_view]
    list_view = list(list_view or [])
    main_list_view = defaults.get_value("main_list_view", main_list_view)
    if main_list_view:
        list_view.append(main_list_view)
    subproject_list_view = defaults.get_value("subproject_list_view", subproject_list_view)
    if subproject_list_view:
        list_view.append(subproject_list_view)

    # in CQ/try, disable ATS on windows. http://b/183895446
    goma_enable_ats = defaults.get_value_from_kwargs("goma_enable_ats", kwargs)
    os = defaults.get_value_from_kwargs("os", kwargs)
    if os and os.category == os_category.WINDOWS:
        if goma_enable_ats == args.COMPUTE:
            kwargs["goma_enable_ats"] = False
        if kwargs["goma_enable_ats"] != False:
            fail("Try Windows builder {} must disable ATS".format(name))

    properties = kwargs.pop("properties", {})
    properties = dict(properties)
    check_for_flakiness = defaults.get_value(
        "check_for_flakiness",
        check_for_flakiness,
    )
    if check_for_flakiness:
        properties["$build/flakiness"] = {
            "check_for_flakiness": True,
        }

    # Define the builder first so that any validation of luci.builder arguments
    # (e.g. bucket) occurs before we try to use it
    builders.builder(
        name = name,
        branch_selector = branch_selector,
        list_view = list_view,
        resultdb_bigquery_exports = merged_resultdb_bigquery_exports,
        experiments = experiments,
        resultdb_index_by_timestamp = True,
        properties = properties,
        **kwargs
    )

    bucket = defaults.get_value_from_kwargs("bucket", kwargs)
    builder = "{}/{}".format(bucket, name)
    cq_group = defaults.get_value("cq_group", cq_group)
    if tryjob != None:
        location_regexp_exclude = tryjob.location_regexp_exclude
        if tryjob.add_default_excludes:
            location_regexp_exclude = DEFAULT_EXCLUDE_REGEXPS + (location_regexp_exclude or [])

        luci.cq_tryjob_verifier(
            builder = builder,
            cq_group = cq_group,
            disable_reuse = tryjob.disable_reuse,
            experiment_percentage = tryjob.experiment_percentage,
            location_regexp = tryjob.location_regexp,
            location_regexp_exclude = location_regexp_exclude,
            cancel_stale = tryjob.cancel_stale,
        )
    else:
        # Allow CQ to trigger this builder if user opts in via CQ-Include-Trybots.
        luci.cq_tryjob_verifier(
            builder = builder,
            cq_group = cq_group,
            includable_only = True,
        )

def _gpu_optional_tests_builder(*, name, **kwargs):
    kwargs.setdefault("builderless", False)
    kwargs.setdefault("execution_timeout", 6 * time.hour)
    kwargs.setdefault("service_account", try_.gpu.SERVICE_ACCOUNT)
    return try_.builder(name = name, **kwargs)

# TODO(gbeaty) Replace with separate functions for orchestrator and compilator
# so that the arguments interact with the module-level defaults in a more
# standard manner
def orchestrator_pair_builders(
        *,
        name,
        orchestrator_cores = None,
        orchestrator_tryjob = None,
        compilator_cores = None,
        compilator_name = None,
        compilator_goma_jobs = None,
        compilator_grace_period = None,
        compilator_builderless = not settings.is_main,
        orchestrator_builderless = not settings.is_main,
        **kwargs):
    builder_group = defaults.get_value_from_kwargs("builder_group", kwargs)
    if not builder_group:
        fail("builder_group must be specified")

    common_description = kwargs.pop("description_html", "")
    if common_description:
        common_description += "<br>"
    orchestrator_url = builder_url("try", name)
    compilator_url = builder_url("try", compilator_name)
    orchestrator_description = common_description + (
        "This is the orchestrator half of an orchestrator + compilator pair of " +
        "builders. The compilator is <a href=\"{}\">{}</a>.".format(
            compilator_url,
            compilator_name,
        )
    )
    compilator_description = common_description + (
        "This is the compilator half of an orchestrator + compilator pair of " +
        "builders. The orchestrator is <a href=\"{}\">{}</a>.".format(
            orchestrator_url,
            name,
        )
    )

    orchestrator_kwargs = dict(kwargs)
    orchestrator_kwargs.update(dict(
        description_html = orchestrator_description,
        executable = "recipe:chromium/orchestrator",
        # TODO(gbeaty) After prod freeze, remove goma details, the
        # orchestrator doesn't compile
        # goma_backend = None,
        os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
        properties = {
            "$build/chromium_orchestrator": {
                "compilator": compilator_name,
                "compilator_watcher_git_revision": compilator_watcher_git_revision,
            },
        },
        service_account = "chromium-orchestrator@chops-service-accounts.iam.gserviceaccount.com",
        ssd = None,
    ))
    orchestrator_builder = try_.builder(
        name = name,
        cores = orchestrator_cores,
        builderless = orchestrator_builderless,
        tryjob = orchestrator_tryjob,
        **orchestrator_kwargs
    )

    compilator_kwargs = dict(kwargs)
    compilator_kwargs.update(dict(
        executable = "recipe:chromium/compilator",
        properties = {
            "orchestrator": {
                "builder_name": name,
                "builder_group": builder_group,
            },
        },
        ssd = True,
    ))
    compilator_builder = try_.builder(
        name = compilator_name,
        cores = compilator_cores,
        builderless = compilator_builderless,
        goma_jobs = compilator_goma_jobs,
        description_html = compilator_description,
        grace_period = compilator_grace_period,
        **compilator_kwargs
    )
    return orchestrator_builder, compilator_builder

try_ = struct(
    # Module-level defaults for try functions
    defaults = defaults,

    # Functions for declaring try builders
    builder = try_builder,
    job = tryjob,
    orchestrator_pair_builders = orchestrator_pair_builders,

    # CONSTANTS
    DEFAULT_EXECUTABLE = "recipe:chromium_trybot",
    DEFAULT_EXECUTION_TIMEOUT = 4 * time.hour,
    DEFAULT_POOL = "luci.chromium.try",
    DEFAULT_SERVICE_ACCOUNT = "chromium-try-builder@chops-service-accounts.iam.gserviceaccount.com",
    gpu = struct(
        optional_tests_builder = _gpu_optional_tests_builder,
        SERVICE_ACCOUNT = "chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
    ),
)
