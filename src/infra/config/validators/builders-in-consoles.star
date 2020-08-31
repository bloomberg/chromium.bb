def _validate_builders_in_console(ctx):
  builders = {}

  for console in ctx.output['luci-milo.cfg'].consoles:
    for builder in console.builders:
      _, long_bucket, builder_name = builder.name.split('/')
      _, _, bucket = long_bucket.split('.', 2)
      builders.setdefault(bucket, {})[builder_name] = True

  builders_without_console = []

  for bucket in ctx.output['cr-buildbucket.cfg'].buckets:
    bucket_builders = builders.get(bucket.name, {})
    for builder in bucket.swarming.builders:
      if builder.name not in bucket_builders:
        builders_without_console.append(
            '{}/{}'.format(bucket.name, builder.name))

  if builders_without_console:
    fail('The following builders do not appear in any console:\n  '
         + '\n  '.join([repr(b) for b in builders_without_console]))

lucicfg.generator(_validate_builders_in_console)
