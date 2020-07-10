# Emulate the bare minimum for chardet for the Swarming bot.
# In practice, we do not need it, and it's very large.
__version__ = '3.0.2'
def detect(_ignored):
  return {'encoding': 'utf-8'}
