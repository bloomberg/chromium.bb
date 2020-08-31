settings = struct(
    is_master = False,
    ref = 'refs/branch-heads/4147',
    ci_bucket = 'ci',
    ci_poller = 'chromium-trigger',
    main_console_name = 'main',
    main_console_title = 'M84 Main Console',
    try_bucket = 'try',
    cq_group = 'cq',
    cq_ref_regexp = 'refs/branch-heads/4147',
    main_list_view_name = 'try',
    main_list_view_title = 'M84 CQ console',
    tree_status_host = None,
)

def master_only_exec(f):
  if settings.is_master:
    exec(f)
