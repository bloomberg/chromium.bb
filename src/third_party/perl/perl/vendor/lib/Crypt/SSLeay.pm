package Crypt::SSLeay;

use strict;
use vars '$VERSION';
$VERSION = '0.72';
$VERSION = eval $VERSION;

eval {
    require XSLoader;
    XSLoader::load('Crypt::SSLeay', $VERSION);
    1;
}
or do {
    require DynaLoader;
    use vars '@ISA'; # not really locally scoped, it just looks that way
    @ISA = qw(DynaLoader);
    bootstrap Crypt::SSLeay $VERSION;
};

use vars qw(%CIPHERS);
%CIPHERS = (
   'NULL-MD5'     => "No encryption with a MD5 MAC",
   'RC4-MD5'      => "128 bit RC4 encryption with a MD5 MAC",
   'EXP-RC4-MD5'  => "40 bit RC4 encryption with a MD5 MAC",
   'RC2-CBC-MD5'  => "128 bit RC2 encryption with a MD5 MAC",
   'EXP-RC2-CBC-MD5' => "40 bit RC2 encryption with a MD5 MAC",
   'IDEA-CBC-MD5' => "128 bit IDEA encryption with a MD5 MAC",
   'DES-CBC-MD5'  => "56 bit DES encryption with a MD5 MAC",
   'DES-CBC-SHA'  => "56 bit DES encryption with a SHA MAC",
   'DES-CBC3-MD5' => "192 bit EDE3 DES encryption with a MD5 MAC",
   'DES-CBC3-SHA' => "192 bit EDE3 DES encryption with a SHA MAC",
   'DES-CFB-M1'   => "56 bit CFB64 DES encryption with a one byte MD5 MAC",
);

use Crypt::SSLeay::X509;

# A xsupp bug made this necessary
sub Crypt::SSLeay::CTX::DESTROY  { shift->free; }
sub Crypt::SSLeay::Conn::DESTROY { shift->free; }
sub Crypt::SSLeay::X509::DESTROY { shift->free; }

1;

__END__

=head1 NAME

Crypt::SSLeay - OpenSSL support for LWP

=head1 HEARTBLEED WARNING

C<perl Makefile.PL> will display a warning if it thinks your OpenSSL might be vulnerable to the  L<Heartbleed Bug|https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2014-0160>. You can, of course, go ahead and install the module, but you should be aware that your system might be exposed to an extremely serious vulnerability. This is just a heuristic based on the version reported by OpenSSL. It is entirely possible that your distrbution actually pushed a patched library, so if you have concerns, you should investigate further.

=head1 SYNOPSIS

    use Net::SSL;
    use LWP::UserAgent;

    my $ua  = LWP::UserAgent->new(
        ssl_opts => { verify_hostname => 0 },
    );

    my $response = $ua->get('https://www.example.com/');
    print $response->content, "\n";

=head1 DESCRIPTION

This Perl module provides support for the HTTPS protocol under L<LWP>, to
allow an L<LWP::UserAgent> object to perform GET, HEAD, and POST requests
over encrypted socket connections. Please see L<LWP> for more information
on POST requests.

The C<Crypt::SSLeay> package provides C<Net::SSL>, which, if requested, is
loaded by C<LWP::Protocol::https> for https requests and provides the
necessary SSL glue.

This distribution also makes following deprecated modules available:

    Crypt::SSLeay::CTX
    Crypt::SSLeay::Conn
    Crypt::SSLeay::X509

=head1 DO YOU NEED Crypt::SSLeay?

Starting with version 6.02 of L<LWP>, C<https> support was unbundled into
L<LWP::Protocol::https>. This module specifies as one of its prerequisites
L<IO::Socket::SSL> which is automatically used by L<LWP::UserAgent> unless
this preference is overridden separately. C<IO::Socket::SSL> is a more
complete implementation, and, crucially, it allows hostname verification.
C<Crypt::SSLeay> does not support this. At this point, C<Crypt::SSLeay> is
maintained to support existing software that already depends on it.
However, it is possible that your software does not really depend on
C<Crypt::SSLeay>, only on the ability of C<LWP::UserAgent> class to
communicate with sites over SSL/TLS.

If are using version C<LWP> 6.02 or later, and therefore have installed
C<LWP::Protocol::https> and its dependencies, and do not explicitly C<use>
C<Net::SSL> before loading C<LWP::UserAgent>, or override the default socket
class, you are probably using C<IO::Socket::SSL> and do not really need
C<Crypt::SSLeay>.

If you have both C<Crypt::SSLeay> and C<IO::Socket::SSL> installed, and
would like to force C<LWP::UserAgent> to use C<Crypt::SSLeay>, you can
use:

    use Net::HTTPS;
    $Net::HTTPS::SSL_SOCKET_CLASS = 'Net::SSL';
    use LWP::UserAgent;

or

    local $ENV{PERL_NET_HTTPS_SSL_SOCKET_CLASS} = 'Net::SSL';
    use LWP::UserAgent;

or

    use Net::SSL;
    use LWP::UserAgent;

=head1 ENVIRONMENT VARIABLES

=over 4

=item Specify SSL Socket Class

C<$ENV{PERL_NET_HTTPS_SSL_SOCKET_CLASS}> can be used to instruct
C<LWP::UserAgent> to use C<Net::SSL> for HTTPS support rather than
C<IO::Socket::SSL>.

=item Proxy Support

    $ENV{HTTPS_PROXY} = 'http://proxy_hostname_or_ip:port';

=item Proxy Basic Authentication

    $ENV{HTTPS_PROXY_USERNAME} = 'username';
    $ENV{HTTPS_PROXY_PASSWORD} = 'password';

=item SSL diagnostics and Debugging

    $ENV{HTTPS_DEBUG} = 1;

=item Default SSL Version

    $ENV{HTTPS_VERSION} = '3';

=item Client Certificate Support

    $ENV{HTTPS_CERT_FILE} = 'certs/notacacert.pem';
    $ENV{HTTPS_KEY_FILE}  = 'certs/notacakeynopass.pem';

=item CA cert Peer Verification

    $ENV{HTTPS_CA_FILE}   = 'certs/ca-bundle.crt';
    $ENV{HTTPS_CA_DIR}    = 'certs/';

=item Client PKCS12 cert support

    $ENV{HTTPS_PKCS12_FILE}     = 'certs/pkcs12.pkcs12';
    $ENV{HTTPS_PKCS12_PASSWORD} = 'PKCS12_PASSWORD';

=back

=head1 INSTALL

=head2 OpenSSL

You must have OpenSSL installed before compiling this module. You can get
the latest OpenSSL package from L<https://www.openssl.org/source/>. We no
longer support pre-2000 versions of OpenSSL.

If you are building OpenSSL from source, please follow the directions
included in the source package.

=head2 Crypt::SSLeay via Makefile.PL

C<Makefile.PL> accepts the following command line arguments:

=over 4

=item C<incpath>

Path to OpenSSL headers. Can also be specified via C<$ENV{OPENSSL_INCLUDE}>.
If the command line argument is provided, it overrides any value specified
via the environment variable. Of course, you can ignore both the command
line argument and the environment variable, and just add the path to your
compiler specific environment variable such as C<CPATH> or C<INCLUDE> etc.

=item C<libpath>

Path to OpenSSL libraries. Can also be specified via C<$ENV{OPENSSL_LIB}>.
If the command line argument is provided, it overrides any value specified
by the environment variable. Of course, you can ignore both the command line
argument and the environment variable and just add the path to your compiler
specific environment variable such as C<LIBRARY_PATH> or C<LIB> etc.

=item C<live-tests>

Use C<--live-tests> to request tests that try to connect to an external web
site, and C<--no-live_tests> to prevent such tests from running. If you run
C<Makefile.PL> interactively, and this argument is not specified on the
command line, you will be prompted for a value.

Default is false.

=item C<static>

Boolean. Default is false. B<TODO>: Does it work?

=item C<verbose>

Boolean. Default is false. If you pass C<--verbose> on the command line,
both C<Devel::CheckLib> and C<ExtUtils::CBuilder> instances will be
configured to echo what they are doing.

=back

If everything builds OK, but you get failures when during tests, ensure that
C<LD_LIBRARY_PATH> points to the location where the correct shared libraries
are located.

If you are using a custom OpenSSL build, please keep in mind that
C<Crypt::SSLeay> must be built using the same compiler and build tools used
to build C<perl> and OpenSSL. This can be more of an issue on Windows. If
you are using Active State Perl, install the MinGW package distributed by
them, and build OpenSSL using that before trying to build this module. If
you have built your own Perl using Microsoft SDK tools or IDEs, make sure
you build OpenSSL using the same tools.

Depending on your OS, pre-built OpenSSL packages may be available. To get
the require headers and import libraries, you may need to install a
development version of your operating system's OpenSSL library package. The
key is that C<Crypt::SSLeay> makes calls to the OpenSSL library, and how to
do so is specified in the C header files that come with the library. Some
systems break out the header files into a separate package from that of the
libraries. Once the program has been built, you don't need the headers any
more.

=head2 Crypt::SSLeay

The latest Crypt::SSLeay can be found at your nearest CPAN mirror, as well
as L<https://metacpan.org/pod/Crypt::SSLeay>.

Once you have downloaded it, C<Crypt::SSLeay> installs easily using the
standard build process:

    $ perl Makefile.PL
    $ make
    $ make test
    $ make install

or

    $ cpanm Crypt::SSLeay

If you have OpenSSL headers and libraries in nonstandard locations, you can
use

    $ perl Makefile.PL --incpath=... --libpath=...

If you would like to use C<cpanm> with such custom locations, you can do

    $ OPENSSL_INCLUDE=... OPENSSL_LIB=... cpanm Crypt::SSLeay

or, on Windows,

    > set OPENSSL_INCLUDE=...
    > set OPENSSL_LIB=...
    > cpanm Crypt::SSLeay

If you are on Windows, and using a MinGW distribution bundled with
ActiveState Perl or Strawberry Perl, you would use C<dmake> rather than
C<make>. If you are using Microsoft's build tools, you would use C<nmake>.

For unattended (batch) installations, to be absolutely certain that
F<Makefile.PL> does not prompt for questions on STDIN, set the environment
variable C<PERL_MM_USE_DEFAULT=1> as with any CPAN module built using
L<ExtUtils::MakeMaker>.

=head3 VMS

I do not have any experience with VMS. If OpenSSL headers and libraries are
not in standard locations searched by your build system by default, please
set things up so that they are. If you have generic instructions on how to
do it, please open a ticket on RT with the information so I can add it to
this document.

=head1 PROXY SUPPORT

L<LWP::UserAgent> and L<Crypt::SSLeay> have their own versions of
proxy support. Please read these sections to see which one
is appropriate.

=head2 LWP::UserAgent proxy support

C<LWP::UserAgent> has its own methods of proxying which may work for you
and is likely to be incompatible with C<Crypt::SSLeay> proxy support.
To use C<LWP::UserAgent> proxy support, try something like:

    my $ua = LWP::UserAgent->new;
    $ua->proxy([qw( https http )], "$proxy_ip:$proxy_port");

At the time of this writing, libwww v5.6 seems to proxy https requests
fine with an Apache F<mod_proxy> server.  It sends a line like:

    GET https://www.example.com HTTP/1.1

to the proxy server, which is not the C<CONNECT> request that some
proxies would expect, so this may not work with other proxy servers than
F<mod_proxy>. The C<CONNECT> method is used by C<Crypt::SSLeay>'s
internal proxy support.

=head2 Crypt::SSLeay proxy support

For native C<Crypt::SSLeay> proxy support of https requests,
you need to set the environment variable C<HTTPS_PROXY> to your
proxy server and port, as in:

    # proxy support
    $ENV{HTTPS_PROXY} = 'http://proxy_hostname_or_ip:port';
    $ENV{HTTPS_PROXY} = '127.0.0.1:8080';

Use of the C<HTTPS_PROXY> environment variable in this way
is similar to C<LWP::UserAgent->env_proxy()> usage, but calling
that method will likely override or break the C<Crypt::SSLeay>
support, so do not mix the two.

Basic auth credentials to the proxy server can be provided
this way:

    # proxy_basic_auth
    $ENV{HTTPS_PROXY_USERNAME} = 'username';
    $ENV{HTTPS_PROXY_PASSWORD} = 'password';

For an example of LWP scripting with C<Crypt::SSLeay> native proxy
support, please look at the F<eg/lwp-ssl-test> script in the
C<Crypt::SSLeay> distribution.

=head1 CLIENT CERTIFICATE SUPPORT

Client certificates are supported. PEM encoded certificate and
private key files may be used like this:

    $ENV{HTTPS_CERT_FILE} = 'certs/notacacert.pem';
    $ENV{HTTPS_KEY_FILE}  = 'certs/notacakeynopass.pem';

You may test your files with the F<eg/net-ssl-test> program,
bundled with the distribution, by issuing a command like:

    perl eg/net-ssl-test -cert=certs/notacacert.pem \
        -key=certs/notacakeynopass.pem -d GET $HOST_NAME

Additionally, if you would like to tell the client where
the CA file is, you may set these.

    $ENV{HTTPS_CA_FILE} = "some_file";
    $ENV{HTTPS_CA_DIR}  = "some_dir";

Note that, if specified, C<$ENV{HTTPS_CA_FILE}> must point to the actual
certificate file. That is, C<$ENV{HTTPS_CA_DIR}> is *not* the path were
C<$ENV{HTTPS_CA_FILE}> is located.

For certificates in C<$ENV{HTTPS_CA_DIR}> to be picked up, follow the
instructions on
L<http://www.openssl.org/docs/ssl/SSL_CTX_load_verify_locations.html>

There is no sample CA cert file at this time for testing,
but you may configure F<eg/net-ssl-test> to use your CA cert
with the -CAfile option.

(TODO: then what is the F<./certs> directory in the distribution?)

=head2 Creating a test certificate

To create simple test certificates with OpenSSL, you may
run the following command:

    openssl req -config /usr/local/openssl/openssl.cnf \
        -new -days 365 -newkey rsa:1024 -x509 \
        -keyout notacakey.pem -out notacacert.pem

To remove the pass phrase from the key file, run:

    openssl rsa -in notacakey.pem -out notacakeynopass.pem

=head2 PKCS12 support

The directives for enabling use of PKCS12 certificates is:

    $ENV{HTTPS_PKCS12_FILE}     = 'certs/pkcs12.pkcs12';
    $ENV{HTTPS_PKCS12_PASSWORD} = 'PKCS12_PASSWORD';

Use of this type of certificate takes precedence over previous
certificate settings described.

(TODO: unclear? Meaning "the presence of this type of certificate"?)

=head1 SSL versions

C<Crypt::SSLeay> tries very hard to connect to I<any> SSL web server
accommodating servers that are buggy, old or simply not standards-compliant.
To this effect, this module will try SSL connections in this order:

=over 4

=item SSL v23

should allow v2 and v3 servers to pick their best type

=item SSL v3

best connection type

=item SSL v2

old connection type

=back

Unfortunately, some servers seem not to handle a reconnect to SSL v3 after a
failed connect of SSL v23 is tried, so you may set before using LWP or
Net::SSL:

    $ENV{HTTPS_VERSION} = 3;

to force a version 3 SSL connection first. At this time only a version 2 SSL
connection will be tried after this, as the connection attempt order remains
unchanged by this setting.

=head1 ACKNOWLEDGEMENTS

Many thanks to the following individuals who helped improve
C<Crypt-SSLeay>:

I<Gisle Aas> for writing this module and many others including libwww, for
perl. The web will never be the same :)

I<Ben Laurie> deserves kudos for his excellent patches for better error
handling, SSL information inspection, and random seeding.

I<Dongqiang Bai> for host name resolution fix when using a proxy.

I<Stuart Horner> of Core Communications, Inc. who found the need for
building C<--shared> OpenSSL libraries.

I<Pavel Hlavnicka> for a patch for freeing memory when using a pkcs12
file, and for inspiring more robust C<read()> behavior.

I<James Woodyatt> is a champ for finding a ridiculous memory leak that
has been the bane of many a Crypt::SSLeay user.

I<Bryan Hart> for his patch adding proxy support, and thanks to I<Tobias
Manthey> for submitting another approach.

I<Alex Rhomberg> for Alpha linux ccc patch.

I<Tobias Manthey> for his patches for client certificate support.

I<Daisuke Kuroda> for adding PKCS12 certificate support.

I<Gamid Isayev> for CA cert support and insights into error messaging.

I<Jeff Long> for working through a tricky CA cert SSLClientVerify issue.

I<Chip Turner> for a patch to build under perl 5.8.0.

I<Joshua Chamas> for the time he spent maintaining the module.

I<Jeff Lavallee> for help with alarms on read failures (CPAN bug #12444).

I<Guenter Knauf> for significant improvements in configuring things in
Win32 and Netware lands and Jan Dubois for various suggestions for
improvements.

and I<many others> who provided bug reports, suggestions, fixes and
patches.

If you have reported a bug or provided feedback, and you would like to be
mentioned by name in this section, please file request on
L<rt.cpan.org|http://rt.cpan.org/NoAuth/Bugs.html?Dist=Crypt-SSLeay>.

=head1 SEE ALSO

=over 4

=item Net::SSL

If you have downloaded this distribution as of a dependency of another
distribution, it's probably due to this module (which is included in
this distribution).

=item Net::SSLeay

L<Net::SSLeay> provides access to the OpenSSL API directly
from Perl. See L<https://metacpan.org/pod/Net::SSLeay/>.

=item Building OpenSSL on 64-bit Windows 8.1 Pro using SDK tools

My blog post L<http://blog.nu42.com/2014/04/building-openssl-101g-on-64-bit-windows.html> might be helpful.

=back

=head1 SUPPORT

For issues related to using of C<Crypt::SSLeay> & C<Net::SSL> with Perl's
L<LWP>, please send email to C<libwww@perl.org>.

For OpenSSL or general SSL support, including issues associated with
building and installing OpenSSL on your system, please email the OpenSSL
users mailing list at C<openssl-users@openssl.org>. See
L<http://www.openssl.org/support/community.html> for other mailing lists
and archives.

Please report all bugs using
L<rt.cpan.org|http://rt.cpan.org/NoAuth/Bugs.html?Dist=Crypt-SSLeay>.

=head1 AUTHORS

This module was originally written by Gisle Aas, and was subsequently
maintained by Joshua Chamas, David Landgren, brian d foy and Sinan Unur.

=head1 COPYRIGHT

Copyright (c) 2010-2014 A. Sinan Unur

Copyright (c) 2006-2007 David Landgren

Copyright (c) 1999-2003 Joshua Chamas

Copyright (c) 1998 Gisle Aas

=head1 LICENSE

This program is free software; you can redistribute it and/or modify it
under the terms of Artistic License 2.0 (see
L<http://www.perlfoundation.org/artistic_license_2_0>).

=cut
