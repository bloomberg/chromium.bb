# TODO(gbeaty) Update the watch to not include active branch heads
# Declare a CQ group that watches all branch heads
# We won't add any builders, but SUBMIT TO CQ fails on Gerrit if there is no CQ
# group
luci.cq_group(
    name = 'fallback-empty-cq',
    retry_config = cq.RETRY_ALL_FAILURES,
    watch = cq.refset(
        repo = 'https://chromium.googlesource.com/chromium/src',
        refs = ['refs/branch-heads/.+'],
    ),
    acls = [
        acl.entry(
            acl.CQ_COMMITTER,
            groups = 'project-chromium-committers',
        ),
        acl.entry(
            acl.CQ_DRY_RUNNER,
            groups = 'project-chromium-tryjob-access',
        ),
    ],
)

# TODO(https://crbug.com/966115) Run a generator to set the fallback field for
# the empty CQ group until it's exposed in lucicfg or there is a better way to
# create a CQ group for all of the canary branches
def _generate_cq_group_fallback(ctx):
  cq_cfg = ctx.output['commit-queue.cfg']

  for c in cq_cfg.config_groups:
    if c.name == 'fallback-empty-cq':
      c.fallback = 1  # YES
      return c

  fail('Could not find empty CQ group')


lucicfg.generator(_generate_cq_group_fallback)
