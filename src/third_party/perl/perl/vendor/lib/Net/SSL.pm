package Net::SSL;

use strict;
use MIME::Base64;
use Socket;
use Carp;

use vars qw(@ISA $VERSION $NEW_ARGS);
$VERSION = '2.86';
$VERSION = eval $VERSION;

require IO::Socket;
@ISA=qw(IO::Socket::INET);

my %REAL; # private to this package only
my $DEFAULT_VERSION = '23';
my $CRLF = "\015\012";
my $SEND_USERAGENT_TO_PROXY = 0;

require Crypt::SSLeay;

sub _default_context {
    require Crypt::SSLeay::MainContext;
    Crypt::SSLeay::MainContext::main_ctx(@_);
}

sub _alarm_set {
    return if $^O eq 'MSWin32' or $^O eq 'NetWare';
    alarm(shift);
}

sub new {
    my($class, %arg) = @_;
    local $NEW_ARGS = \%arg;
    $class->SUPER::new(%arg);
}

sub DESTROY {
    my $self = shift;
    delete $REAL{$self};
    local $@;
    eval { $self->SUPER::DESTROY; };
}

sub configure {
    my($self, $arg) = @_;
    my $ssl_version = delete $arg->{SSL_Version} ||
      $ENV{HTTPS_VERSION} || $DEFAULT_VERSION;
    my $ssl_debug = delete $arg->{SSL_Debug} || $ENV{HTTPS_DEBUG} || 0;

    my $ctx = delete $arg->{SSL_Context} || _default_context($ssl_version);

    *$self->{ssl_ctx} = $ctx;
    *$self->{ssl_version} = $ssl_version;
    *$self->{ssl_debug} = $ssl_debug;
    *$self->{ssl_arg} = $arg;
    *$self->{ssl_peer_addr} = $arg->{PeerAddr};
    *$self->{ssl_peer_port} = $arg->{PeerPort};
    *$self->{ssl_new_arg} = $NEW_ARGS;
    *$self->{ssl_peer_verify} = 0;

    ## Crypt::SSLeay must also aware the SSL Proxy before calling
    ## $socket->configure($args). Because the $sock->configure() will
    ## die when failed to resolve the destination server IP address,
    ## whether the SSL proxy is used or not!
    ## - dqbai, 2003-05-10
    if (my $proxy = $self->proxy) {
        ($arg->{PeerAddr}, $arg->{PeerPort}) = split(':',$proxy);
        $arg->{PeerPort} || croak("no port given for proxy server $proxy");
    }

    $self->SUPER::configure($arg);
}

# override to make sure there is really a timeout
sub timeout {
    shift->SUPER::timeout || 60;
}

sub blocking {
    my $self = shift;
    $self->SUPER::blocking(@_);
}

sub connect {
    my $self = shift;

    # configure certs on connect() time, so we can throw an undef
    # and have LWP understand the error
    eval { $self->configure_certs() };
    if($@) {
        $@ = "configure certs failed: $@; $!";
        $self->die_with_error($@);
    }

    # finished, update set_verify status
    if(my $rv = *$self->{ssl_ctx}->set_verify()) {
        *$self->{ssl_peer_verify} = $rv;
    }

    if ($self->proxy) {
        # don't die() in connect, just return undef and set $@
        my $proxy_connect = eval { $self->proxy_connect_helper(@_) };
        if(! $proxy_connect || $@) {
            $@ = "proxy connect failed: $@; $!";
            croak($@);
        }
    }
    else {
        *$self->{io_socket_peername}=@_ == 1 ? $_[0] : IO::Socket::sockaddr_in(@_);
        if(!$self->SUPER::connect(@_)) {
            # better to die than return here
            $@ = "Connect failed: $@; $!";
            croak($@);
        }
    }

    my $debug = *$self->{ssl_debug} || 0;
    my $ssl = Crypt::SSLeay::Conn->new(*$self->{ssl_ctx}, $debug, $self);
    my $arg = *$self->{ssl_arg};
    my $new_arg = *$self->{ssl_new_arg};
    $arg->{SSL_Debug} = $debug;

    # setup SNI if available
    $ssl->can("set_tlsext_host_name") and
        $ssl->set_tlsext_host_name(*$self->{ssl_peer_addr});

    eval {
        local $SIG{ALRM} = sub { $self->die_with_error("SSL connect timeout") };
        # timeout / 2 because we have 3 possible connects here
        _alarm_set($self->timeout / 2);

        my $rv;
        {
            local $SIG{PIPE} = \&die;
            $rv = eval { $ssl->connect; };
        }
        if (not defined $rv or $rv <= 0) {
            _alarm_set(0);
            $ssl = undef;
            # See RT #59312
            my %args = (%$arg, %$new_arg);
            if(*$self->{ssl_version} == 23) {
                $args{SSL_Version} = 3;
                # the new connect might itself be overridden with a REAL SSL
                my $new_ssl = Net::SSL->new(%args);
                $REAL{$self} = $REAL{$new_ssl} || $new_ssl;
                return $REAL{$self};
            }
            elsif(*$self->{ssl_version} == 3) {
                # $self->die_with_error("SSL negotiation failed");
                $args{SSL_Version} = 2;
                my $new_ssl = Net::SSL->new(%args);
                $REAL{$self} = $new_ssl;
                return $new_ssl;
            }
			else {
                # don't die, but do set $@, and return undef
                eval { $self->die_with_error("SSL negotiation failed") };
                croak($@);
            }
        }
        _alarm_set(0);
    };

    # odd error in eval {} block, maybe alarm outside the evals
    if($@) {
        $@ = "$@; $!";
        croak($@);
    }

    # successful SSL connection gets stored
    *$self->{ssl_ssl} = $ssl;
    $self;
}

# Delegate these calls to the Crypt::SSLeay::Conn object
sub get_peer_certificate {
    my $self = shift;
    $self = $REAL{$self} || $self;
    *$self->{ssl_ssl}->get_peer_certificate(@_);
}

sub get_peer_verify {
    my $self = shift;
    $self = $REAL{$self} || $self;
    *$self->{ssl_peer_verify};
}

sub get_shared_ciphers {
    my $self = shift;
    $self = $REAL{$self} || $self;
    *$self->{ssl_ssl}->get_shared_ciphers(@_);
}

sub get_cipher {
    my $self = shift;
    $self = $REAL{$self} || $self;
    *$self->{ssl_ssl}->get_cipher(@_);
}

sub pending {
    my $self = shift;
    $self = $REAL{$self} || $self;
    *$self->{ssl_ssl}->pending(@_);
}

sub ssl_context {
    my $self = shift;
    $self = $REAL{$self} || $self;
    *$self->{ssl_ctx};
}

sub die_with_error {
    my $self=shift;
    my $reason=shift;

    my @err;
    while(my $err=Crypt::SSLeay::Err::get_error_string()) {
       push @err, $err;
    }
    croak("$reason: " . join( ' | ', @err ));
}

sub read {
    my $self = shift;
    $self = $REAL{$self} || $self;

    local $SIG{__DIE__} = \&Carp::confess;
    local $SIG{ALRM} = sub { $self->die_with_error("SSL read timeout") };

    _alarm_set($self->timeout);
    my $n = *$self->{ssl_ssl}->read(@_);
    _alarm_set(0);
    $self->die_with_error("read failed") if !defined $n;

    $n;
}

sub write {
    my $self = shift;
    $self = $REAL{$self} || $self;
    my $n = *$self->{ssl_ssl}->write(@_);
    $self->die_with_error("write failed") if !defined $n;
    $n;
}

*sysread  = \&read;
*syswrite = \&write;

sub print {
    my $self = shift;
    $self = $REAL{$self} || $self;
    # should we care about $, and $\??
    # I think it is too expensive...
    $self->write(join("", @_));
}

sub printf {
    my $self = shift;
    $self = $REAL{$self} || $self;
    my $fmt = shift;
    $self->write(sprintf($fmt, @_));
}

sub getchunk {
    my $self = shift;
    $self = $REAL{$self} || $self;
    my $buf = '';  # warnings
    my $n = $self->read($buf, 32768);
    return unless defined $n;
    $buf;
}

# This is really inefficient, but we only use it for reading the proxy response
# so that does not really matter.
sub getline {
    my $self = shift;
    $self = $REAL{$self} || $self;
    my $val="";
    my $buf;
    do {
        $self->SUPER::recv($buf, 1);
        $val .= $buf;
    } until ($buf eq "\n");

    $val;
}

# XXX: no way to disable <$sock>??  (tied handle perhaps?)

sub get_lwp_object {
    my $self = shift;

    my $lwp_object;
    my $i = 0;
    while(1) {
        package DB;
        my @stack = caller($i++);
        last unless @stack;
        my @stack_args = @DB::args;
        my $stack_object = $stack_args[0] || next;
        return $stack_object
            if ref($stack_object)
                and $stack_object->isa('LWP::UserAgent');
    }
    return undef;
}

sub send_useragent_to_proxy {
    if (my $val = shift) {
        $SEND_USERAGENT_TO_PROXY = $val;
    }
    return $SEND_USERAGENT_TO_PROXY;
}

sub proxy_connect_helper {
    my $self = shift;

    my $proxy = $self->proxy;
    my ($proxy_host, $proxy_port) = split(':',$proxy);
    $proxy_port || croak("no port given for proxy server $proxy");

    my $proxy_addr = gethostbyname($proxy_host);
    $proxy_addr || croak("can't resolve proxy server name: $proxy_host, $!");

    my($peer_port, $peer_addr) = (*$self->{ssl_peer_port}, *$self->{ssl_peer_addr});
    $peer_addr || croak("no peer addr given");
    $peer_port || croak("no peer port given");

    # see if the proxy should be bypassed
    my @no_proxy = split( /\s*,\s*/, $ENV{NO_PROXY} || $ENV{no_proxy} || '');
    my $is_proxied = 1;
    my $domain;
    for $domain (@no_proxy) {
        if ($peer_addr =~ /\Q$domain\E$/) {
            $is_proxied = 0;
            last;
        }
    }

    if ($is_proxied) {
        $self->SUPER::connect($proxy_port, $proxy_addr)
          || croak("proxy connect to $proxy_host:$proxy_port failed: $!");
    }
    else {
        # see RT #57836
        my $peer_addr_packed = gethostbyname($peer_addr);
        $self->SUPER::connect($peer_port, $peer_addr_packed)
          || croak("proxy bypass to $peer_addr:$peer_addr failed: $!");
    }

    my $connect_string;
    if ($ENV{"HTTPS_PROXY_USERNAME"} || $ENV{"HTTPS_PROXY_PASSWORD"}) {
        my $user = $ENV{"HTTPS_PROXY_USERNAME"};
        my $pass = $ENV{"HTTPS_PROXY_PASSWORD"};

        my $credentials = encode_base64("$user:$pass", "");
        $connect_string = join($CRLF,
            "CONNECT $peer_addr:$peer_port HTTP/1.0",
            "Proxy-authorization: Basic $credentials"
        );
    }
    else {
        $connect_string = "CONNECT $peer_addr:$peer_port HTTP/1.0";
    }
    $connect_string .= $CRLF;

    if (send_useragent_to_proxy()) {
        my $lwp_object = $self->get_lwp_object;
        if($lwp_object && $lwp_object->agent) {
            $connect_string .= "User-Agent: ".$lwp_object->agent.$CRLF;
        }
    }

    $connect_string .= $CRLF;
    $self->SUPER::send($connect_string);

    my $timeout;
    my $header = '';

    # See RT #33954
    # See also RT #64054
    # Handling incomplete reads and writes better (for some values of
    # better) may actually make this problem go away, but either way,
    # there is no good reason to use \d when checking for 0-9

    while ($header !~ m{HTTP/[0-9][.][0-9]\s+200\s+.*$CRLF$CRLF}s) {
        $timeout = $self->timeout(5) unless length $header;
        my $n = $self->SUPER::sysread($header, 8192, length $header);
        last if $n <= 0;
    }

    $self->timeout($timeout) if defined $timeout;
    my $conn_ok = ($header =~ m{HTTP/[0-9]+[.][0-9]+\s+200\s+}is) ? 1 : 0;

    if (not $conn_ok) {
        croak("PROXY ERROR HEADER, could be non-SSL URL:\n$header");
    }

    $conn_ok;
}

# code adapted from LWP::UserAgent, with $ua->env_proxy API
# see also RT #57836
sub proxy {
    my $self = shift;
    my $proxy_server = $ENV{HTTPS_PROXY} || $ENV{https_proxy};
    return unless $proxy_server;

    my($peer_port, $peer_addr) = (
        *$self->{ssl_peer_port},
        *$self->{ssl_peer_addr}
    );
    $peer_addr || croak("no peer addr given");
    $peer_port || croak("no peer port given");

    # see if the proxy should be bypassed
    my @no_proxy = split( /\s*,\s*/,
        $ENV{NO_PROXY} || $ENV{no_proxy} || ''
    );
    my $is_proxied = 1;
    for my $domain (@no_proxy) {
        if ($peer_addr =~ /\Q$domain\E\z/) {
            return;
        }
    }

    $proxy_server =~ s|\Ahttps?://||i;
    # sanitize the end of the string too
    # see also http://www.nntp.perl.org/group/perl.libwww/2012/10/msg7629.html
    # and https://github.com/nanis/Crypt-SSLeay/pull/1
    # Thank you Mark Allen and YigangX Wen
    $proxy_server =~ s|(:[1-9][0-9]{0,4})/\z|$1|;
    $proxy_server;
}

sub configure_certs {
    my $self = shift;
    my $ctx = *$self->{ssl_ctx};

    my $count = 0;
    for (qw(HTTPS_PKCS12_FILE HTTPS_CERT_FILE HTTPS_KEY_FILE)) {
        my $file = $ENV{$_};
        if ($file) {
            (-e $file) or croak("$file file does not exist: $!");
            (-r $file) or croak("$file file is not readable");
            $count++;
            if (/PKCS12/) {
                $count++;
                $ctx->use_pkcs12_file($file ,$ENV{'HTTPS_PKCS12_PASSWORD'}) || croak("failed to load $file: $!");
                last;
            }
            elsif (/CERT/) {
                $ctx->use_certificate_file($file ,1) || croak("failed to load $file: $!");
            }
            elsif (/KEY/) {
                $ctx->use_PrivateKey_file($file, 1) || croak("failed to load $file: $!");
            }
            else {
                croak("setting $_ not supported");
            }
        }
    }

    # if both configs are set, then verify them
    if ($count == 2) {
        if (! $ctx->check_private_key) {
            croak("Private key and certificate do not match");
        }
    }

    $count; # number of successful cert loads/checks
}

sub accept   { shift->_unimpl("accept") }
sub getc     { shift->_unimpl("getc")   }
sub ungetc   { shift->_unimpl("ungetc") }
sub getlines { shift->_unimpl("getlines"); }

sub _unimpl {
    my($self, $meth) = @_;
    croak("$meth not implemented for Net::SSL sockets");
}

1;

__END__

=head1 NAME

Net::SSL - support for Secure Sockets Layer

=head1 METHODS

=over 4

=item new

Creates a new C<Net::SSL> object.

=item configure

Configures a C<Net::SSL> socket for operation.

=item configure_certs

Sets up a certificate file to use for communicating with on
the socket.

=item connect

=item die_with_error

=item get_cipher

=item get_lwp_object

Walks up the caller stack and looks for something blessed into
the C<LWP::UserAgent> namespace and returns it. Vaguely deprecated.

=item get_peer_certificate

Gets the peer certificate from the underlying C<Crypt::SSLeay::Conn>
object.

=item get_peer_verify

=item get_shared_ciphers

=item getchunk

Attempts to read up to 32KiB of data from the socket. Returns
C<undef> if nothing was read, otherwise returns the data as
a scalar.

=item pending

Provides access to OpenSSL's C<pending> attribute on the SSL connection
object.

=item getline

Reads one character at a time until a newline is encountered,
and returns the line, including the newline. Grossly
inefficient.

=item print

Concatenates the input parameters and writes them to the socket.
Does not honour C<$,> nor C<$/>. Returns the number of bytes written.

=item printf

Performs a C<sprintf> of the input parameters (thus, the first
parameter must be the format), and writes the result to the socket.
Returns the number of bytes written.

=item proxy

Returns the hostname of an https proxy server, as specified by the
C<HTTPS_PROXY> environment variable.

=item proxy_connect_helper

Helps set up a connection through a proxy.

=item read

Performs a read on the socket and returns the result.

=item ssl_context

=item sysread

Is an alias of C<read>.

=item timeout

Returns the timeout value of the socket as defined by the implementing
class or 60 seconds by default.

=item blocking

Returns a boolean indicating whether the underlying socket is in
blocking mode. By default, Net::SSL sockets are in blocking mode.

    $sock->blocking(0); # set to non-blocking mode

This method simply calls the underlying C<blocking> method of the
IO::Socket object.

=item write

Writes the parameters passed in (thus, a list) to the socket. Returns
the number of bytes written.

=item syswrite

Is an alias of C<write>.

=item accept

Not yet implemented. Will die if called.

=item getc

Not yet implemented. Will die if called.

=item getlines

Not yet implemented. Will die if called.

=item ungetc

Not yet implemented. Will die if called.

=item send_useragent_to_proxy

By default (as of version 2.80 of C<Net::SSL> in the 0.54 distribution
of Crypt::SSLeay), the user agent string is no longer sent to the
proxy (but will continue to be sent to the remote host).

The previous behaviour was of marginal benefit, and could cause
fatal errors in certain scenarios (see CPAN bug #4759) and so no
longer happens by default.

To reinstate the old behaviour, call C<Net::SSL::send_useragent_to_proxy>
with a true value (usually 1).

=back

=head1 DIAGNOSTICS

  "no port given for proxy server <proxy>"

A proxy was specified for configuring a socket, but no port number
was given. Ensure that the proxy is specified as a host:port pair,
such as C<proxy.example.com:8086>.

  "configure certs failed: <contents of $@>; <contents of $!>"

  "proxy connect failed: <contents of $@>; <contents of $!>"

  "Connect failed: <contents of $@>; <contents of $!>"

During connect().

=head2 SEE ALSO

=over 4

=item IO::Socket::INET

C<Net::SSL> is implemented by subclassing C<IO::Socket::INET>, hence
methods not specifically overridden are defined by that package.

=item Net::SSLeay

A package that provides a Perl-level interface to the C<openssl>
secure sockets layer library.

=back

=cut

