"""Classes for integrating TLS Lite with other packages."""

__all__ = ["asyncstatemachine",
           "httptlsconnection",
           "pop3_tls",
           "imap4_tls",
           "smtp_tls",
           "xmlrpctransport",
           "tlssocketservermixin",
           "tlsasyncdispatchermixin",
           "tlstwistedprotocolwrapper"]

try:
    import twisted
    del twisted
except ImportError:
   del __all__[__all__.index("tlstwistedprotocolwrapper")]
