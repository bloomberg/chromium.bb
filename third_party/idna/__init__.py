# Emulate the bare minimum for idna for the Swarming bot.
# In practice, we do not need it, and it's very large.
def encode(host, uts46):
  return unicode(host)
