package Net::SSH2;

use 5.006;
use strict;
use warnings;
use Carp;

require Exporter;
use AutoLoader;

use Socket;
use IO::File;
use File::Basename;

use base 'Exporter';

# constants

my @EX_callback = qw(
        LIBSSH2_CALLBACK_DEBUG
        LIBSSH2_CALLBACK_DISCONNECT
        LIBSSH2_CALLBACK_IGNORE
        LIBSSH2_CALLBACK_MACERROR
        LIBSSH2_CALLBACK_X11
);

my @EX_channel = qw(
        LIBSSH2_CHANNEL_EXTENDED_DATA_IGNORE
        LIBSSH2_CHANNEL_EXTENDED_DATA_MERGE
        LIBSSH2_CHANNEL_EXTENDED_DATA_NORMAL
);

my @EX_socket = qw(
        LIBSSH2_SOCKET_BLOCK_INBOUND
        LIBSSH2_SOCKET_BLOCK_OUTBOUND
);

my @EX_trace = qw(
        LIBSSH2_TRACE_TRANS
        LIBSSH2_TRACE_KEX
        LIBSSH2_TRACE_AUTH
        LIBSSH2_TRACE_CONN
        LIBSSH2_TRACE_SCP
        LIBSSH2_TRACE_SFTP
        LIBSSH2_TRACE_ERROR
        LIBSSH2_TRACE_PUBLICKEY
        LIBSSH2_TRACE_SOCKET
);

my @EX_error = qw(
        LIBSSH2_ERROR_ALLOC
        LIBSSH2_ERROR_BANNER_NONE
        LIBSSH2_ERROR_BANNER_SEND
        LIBSSH2_ERROR_CHANNEL_CLOSED
        LIBSSH2_ERROR_CHANNEL_EOF_SENT
        LIBSSH2_ERROR_CHANNEL_FAILURE
        LIBSSH2_ERROR_CHANNEL_OUTOFORDER
        LIBSSH2_ERROR_CHANNEL_PACKET_EXCEEDED
        LIBSSH2_ERROR_CHANNEL_REQUEST_DENIED
        LIBSSH2_ERROR_CHANNEL_UNKNOWN
        LIBSSH2_ERROR_CHANNEL_WINDOW_EXCEEDED
        LIBSSH2_ERROR_DECRYPT
        LIBSSH2_ERROR_FILE
        LIBSSH2_ERROR_HOSTKEY_INIT
        LIBSSH2_ERROR_HOSTKEY_SIGN
        LIBSSH2_ERROR_INVAL
        LIBSSH2_ERROR_INVALID_MAC
        LIBSSH2_ERROR_INVALID_POLL_TYPE
        LIBSSH2_ERROR_KEX_FAILURE
        LIBSSH2_ERROR_KEY_EXCHANGE_FAILURE
        LIBSSH2_ERROR_METHOD_NONE
        LIBSSH2_ERROR_METHOD_NOT_SUPPORTED
        LIBSSH2_ERROR_PASSWORD_EXPIRED
        LIBSSH2_ERROR_PROTO
        LIBSSH2_ERROR_PUBLICKEY_UNRECOGNIZED
        LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED
        LIBSSH2_ERROR_REQUEST_DENIED
        LIBSSH2_ERROR_SCP_PROTOCOL
        LIBSSH2_ERROR_SFTP_PROTOCOL
        LIBSSH2_ERROR_SOCKET_DISCONNECT
        LIBSSH2_ERROR_SOCKET_NONE
        LIBSSH2_ERROR_SOCKET_SEND
        LIBSSH2_ERROR_SOCKET_TIMEOUT
        LIBSSH2_ERROR_TIMEOUT
        LIBSSH2_ERROR_ZLIB
        LIBSSH2_ERROR_EAGAIN
);

my @EX_hash = qw(
        LIBSSH2_HOSTKEY_HASH_MD5
        LIBSSH2_HOSTKEY_HASH_SHA1
);

my @EX_method = qw(
        LIBSSH2_METHOD_COMP_CS
        LIBSSH2_METHOD_COMP_SC
        LIBSSH2_METHOD_CRYPT_CS
        LIBSSH2_METHOD_CRYPT_SC
        LIBSSH2_METHOD_HOSTKEY
        LIBSSH2_METHOD_KEX
        LIBSSH2_METHOD_LANG_CS
        LIBSSH2_METHOD_LANG_SC
        LIBSSH2_METHOD_MAC_CS
        LIBSSH2_METHOD_MAC_SC
);

my @EX_fxf = qw(
        LIBSSH2_FXF_APPEND
        LIBSSH2_FXF_CREAT
        LIBSSH2_FXF_EXCL
        LIBSSH2_FXF_READ
        LIBSSH2_FXF_TRUNC
        LIBSSH2_FXF_WRITE
);

my @EX_fx = qw(
        LIBSSH2_FX_BAD_MESSAGE
        LIBSSH2_FX_CONNECTION_LOST
        LIBSSH2_FX_DIR_NOT_EMPTY
        LIBSSH2_FX_EOF
        LIBSSH2_FX_FAILURE
        LIBSSH2_FX_FILE_ALREADY_EXISTS
        LIBSSH2_FX_INVALID_FILENAME
        LIBSSH2_FX_INVALID_HANDLE
        LIBSSH2_FX_LINK_LOOP
        LIBSSH2_FX_LOCK_CONFlICT
        LIBSSH2_FX_NOT_A_DIRECTORY
        LIBSSH2_FX_NO_CONNECTION
        LIBSSH2_FX_NO_MEDIA
        LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM
        LIBSSH2_FX_NO_SUCH_FILE
        LIBSSH2_FX_NO_SUCH_PATH
        LIBSSH2_FX_OK
        LIBSSH2_FX_OP_UNSUPPORTED
        LIBSSH2_FX_PERMISSION_DENIED
        LIBSSH2_FX_QUOTA_EXCEEDED
        LIBSSH2_FX_UNKNOWN_PRINCIPLE
        LIBSSH2_FX_WRITE_PROTECT
);

my @EX_sftp = qw(
        LIBSSH2_SFTP_ATTR_ACMODTIME
        LIBSSH2_SFTP_ATTR_EXTENDED
        LIBSSH2_SFTP_ATTR_PERMISSIONS
        LIBSSH2_SFTP_ATTR_SIZE
        LIBSSH2_SFTP_ATTR_UIDGID
        LIBSSH2_SFTP_LSTAT
        LIBSSH2_SFTP_OPENDIR
        LIBSSH2_SFTP_OPENFILE
        LIBSSH2_SFTP_PACKET_MAXLEN
        LIBSSH2_SFTP_READLINK
        LIBSSH2_SFTP_REALPATH
        LIBSSH2_SFTP_RENAME_ATOMIC
        LIBSSH2_SFTP_RENAME_NATIVE
        LIBSSH2_SFTP_RENAME_OVERWRITE
        LIBSSH2_SFTP_SETSTAT
        LIBSSH2_SFTP_STAT
        LIBSSH2_SFTP_SYMLINK
        LIBSSH2_SFTP_TYPE_BLOCK_DEVICE
        LIBSSH2_SFTP_TYPE_CHAR_DEVICE
        LIBSSH2_SFTP_TYPE_DIRECTORY
        LIBSSH2_SFTP_TYPE_FIFO
        LIBSSH2_SFTP_TYPE_REGULAR
        LIBSSH2_SFTP_TYPE_SOCKET
        LIBSSH2_SFTP_TYPE_SPECIAL
        LIBSSH2_SFTP_TYPE_SYMLINK
        LIBSSH2_SFTP_TYPE_UNKNOWN
        LIBSSH2_SFTP_VERSION
);

my @EX_disconnect = qw(
        SSH_DISCONNECT_AUTH_CANCELLED_BY_USER
        SSH_DISCONNECT_BY_APPLICATION
        SSH_DISCONNECT_COMPRESSION_ERROR
        SSH_DISCONNECT_CONNECTION_LOST
        SSH_DISCONNECT_HOST_KEY_NOT_VERIFIABLE
        SSH_DISCONNECT_HOST_NOT_ALLOWED_TO_CONNECT
        SSH_DISCONNECT_ILLEGAL_USER_NAME
        SSH_DISCONNECT_KEY_EXCHANGE_FAILED
        SSH_DISCONNECT_MAC_ERROR
        SSH_DISCONNECT_NO_MORE_AUTH_METHODS_AVAILABLE
        SSH_DISCONNECT_PROTOCOL_ERROR
        SSH_DISCONNECT_PROTOCOL_VERSION_NOT_SUPPORTED
        SSH_DISCONNECT_RESERVED
        SSH_DISCONNECT_SERVICE_NOT_AVAILABLE
        SSH_DISCONNECT_TOO_MANY_CONNECTIONS
);

our %EXPORT_TAGS = (
    all        => [
        @EX_callback, @EX_channel, @EX_error, @EX_socket, @EX_trace, @EX_hash,
        @EX_method, @EX_fx, @EX_fxf, @EX_sftp, @EX_disconnect,
    ],
    # ssh
    callback   => \@EX_callback,
    channel    => \@EX_channel,
    error      => \@EX_error,
    socket     => \@EX_socket,
    trace      => \@EX_trace,
    hash       => \@EX_hash,
    method     => \@EX_method,
    disconnect => \@EX_disconnect,
    # sftp
    fx         => \@EX_fx,
    fxf        => \@EX_fxf,
    sftp       => \@EX_sftp,
);

our @EXPORT_OK = @{$EXPORT_TAGS{all}};

our $VERSION = '0.44';

# methods

sub new {
    my $class = shift;
    my %opts  = @_;

    my $self = $class->_new;

    $self->trace($opts{trace}) if exists $opts{trace};

    return $self;
}

sub connect {
    my $self = shift;
    croak "Net::SSH2::connect: not enough parameters" if @_ < 1;

    my $wantarray = wantarray;

    # try to connect, or get a file descriptor
    my ($fd, $sock);
    if (@_ == 1) {
        $sock = shift;
        if ($sock =~ /^\d{1,10}$/) {
            $fd = $sock;
        } elsif(ref $sock) {
            # handled below
        } else {
            @_ = ($sock, getservbyname(qw(ssh tcp)) || 22);
        }
    }

    my %opts = splice @_, 2 if @_ >= 4;
    $opts{Timeout} ||= 30;

    if (@_ == 2) {
        require IO::Socket::INET;
        $sock = IO::Socket::INET->new(
            PeerHost => $_[0],
            PeerPort => $_[1],
            Timeout => $opts{Timeout},
        );

        if (not $sock) {
            if (not defined $wantarray) {
                croak "Net::SSH2: failed to connect to $_[0]:$_[1]: $!"
            } else {
                return; # to support ->connect ... or die
            }
        }

        $sock->sockopt(SO_LINGER, pack('SS', 0, 0));
    }

    # get a file descriptor
    $fd ||= fileno($sock);
    croak "Net::SSH2::connect: can't get file descriptor for $sock"
     unless defined $fd;
    if ($^O eq 'MSWin32') {
        require Win32API::File;
        $fd = Win32API::File::FdGetOsFHandle($fd);
    }

    # pass it in, do protocol
    return $self->_startup($fd, $sock);
}

sub _auth_methods {
    return {
        ((version())[1]||0 >= 0x010203 ? (
            'agent' => {
                ssh => 'agent',
                method => \&auth_agent,
                params => [qw(username)],
            },
        ) : ()),
        'hostbased'     => {
            ssh    => 'hostbased',
            method => \&auth_hostbased,
            params => [qw(username publickey privatekey
                       hostname local_username? password?)],
        },
        'publickey'     => {
            ssh    => 'publickey',
            method => \&auth_publickey,
            params => [qw(username publickey privatekey password?)],
        },
        'keyboard'      => {
            ssh    => 'keyboard-interactive', 
            method => \&auth_keyboard,
            params => [qw(_interact username cb_keyboard?)]
        },
        'keyboard-auto' => {
            ssh    => 'keyboard-interactive',
            method => \&auth_keyboard,
            params => [qw(username password)],
        },
        'password'      => {
            ssh    => 'password',
            method => \&auth_password,
            params => [qw(username password cb_password?)],
        },
        'none'          => {
            ssh    => 'none',
            method => \&auth_password,
            params => [qw(username)],
        },
    };
}

sub _auth_rank {
    return [
        ((version())[1]||0 >= 0x010203 ? ('agent') : ()),
        qw(hostbased publickey keyboard keyboard-auto password none)
    ];
}

sub auth {
    my ($self, %p) = @_;
    my $rank = delete $p{rank} || $self->_auth_rank;

    TYPE: for(my $i = 0; $i < @$rank; $i++) {
        my $type = $rank->[$i];
        my $data = $self->_auth_methods->{$type};
        confess "unknown authentication method '$type'" unless $data;

        # do we have the required parameters?
        my @pass;
        for my $param(@{$data->{params}}) {
            my $p = $param;
            my $opt = $p =~ s/\?$//;
            my $pseudo = $p =~ s/^_//;
            next TYPE if not $opt and not exists $p{$p};
            next if $pseudo;     # don't push pseudos
            push @pass, $p{$p};  # if it's optional, store undef
        }

        # invoke the authentication method
        return $type if $data->{method}->($self, @pass) and $self->auth_ok;
    }
    return;  # failure
}

sub scp_get {
    my ($self, $remote, $path) = @_;
    $path = basename $remote if not defined $path;

    my %stat;
    my $chan = $self->_scp_get($remote, \%stat);
    return unless $chan;

    # read and commit blocks until we're finished
    $chan->blocking(1);
    my $mode = $stat{mode} & 0777;
    my $file = ref $path ? $path
       : IO::File->new($path, O_WRONLY | O_CREAT | O_TRUNC, $mode);
    return unless $file;

    for (my ($size, $count) = ($stat{size}); $size > 0; $size -= $count) {
      my $buf = '';
      my $block = ($size > 8192) ? 8192 : $size;
      $count = $chan->read($buf, $block);
      return unless defined $count;
      $self->error(0, "read $block bytes but only got $count"), return
       unless $count == $block;
      $self->error(0, "error writing $count bytes to file"), return
       unless $file->syswrite($buf, $count) == $count;
    }

    # process SCP acknowledgment and send same
    my $eof;
    $chan->read($eof, 1);
    $chan->write("\0");
    undef $chan;  # close
    return 1;
}

sub scp_put {
    my ($self, $path, $remote) = @_;
    $remote = basename $path if not defined $remote;

    my $file = ref $path ? $path : IO::File->new($path, O_RDONLY);
    $self->error($!, $!), return unless $file;
    my @stat = $file->stat;
    $self->error($!, $!), return unless @stat;

    my $mode = $stat[2] & 0777;  # mask off extras such as S_IFREG
    my $chan = $self->_scp_put($remote, $mode, @stat[7, 8, 9]);
    return unless $chan;
    $chan->blocking(1);

    # read and transmit blocks until we're finished
    for (my ($size, $count) = ($stat[7]); $size > 0; $size -= $count) {
      my $buf;
      my $block = ($size > 8192) ? 8192 : $size;
      $count = $file->sysread($buf, $block);
      $self->error($!, $!), return unless defined $count;
      $self->error(0, "want $block, have $count"), return
       unless $count == $block;
      die 'sysread mismatch' unless length $buf == $count;
      my $wrote = 0;
      while ($wrote >= 0 && $wrote < $count) {
        my $wr = $chan->write($buf);
        last if $wr < 0;
        $wrote += $wr;
        $buf = substr $buf, $wr;
      }
      $self->error(0, "error writing $count bytes to channel"), return
       unless $wrote == $count;
    }

    # send/receive SCP acknowledgement
    $chan->write("\0");
    my $eof;
    $chan->read($eof, 1);
    return 1;
}

my %Event;

sub _init_poll {
    for my $event(qw(
     pollin pollpri pollext pollout pollerr pollhup pollnval pollex
     session_closed channel_closed listener_closed
    )) {
        no strict 'refs';
        my $name = 'LIBSSH2_POLLFD_'.uc($event);
        (my $_event = $event) =~ s/^poll//;
        $Event{$_event} = &$name;
    }
}

sub poll {
    my ($self, $timeout, $event) = @_;
    $timeout ||= 0;

    # map incoming event structure (files to handles, events to integers)
    my @event;
    for my $in (@$event) {
        my ($handle, $events) = @{$in}{qw(handle events)};
        $handle = fileno $handle
         unless ref $handle and ref($handle) =~ /^Net::SSH2::/;
        my $out = { handle => $handle, events => 0 };
        $events = [$events] if not ref $events and $events =~ /^\D+$/;
        if (UNIVERSAL::isa($events, 'ARRAY')) {
            for my $name(@$events) {
                my $value = $Event{$name};
                croak "Net::SSH2::poll: can't translate event '$name'"
                 unless defined $value;
                $out->{events} |= $value;
            }
        } else {
            $out->{events} = $events || 0;
        }
        push @event, $out;
    }

    my $count = $self->_poll($timeout, \@event);
    return if not defined $count;

    # map received event structure (bitmask to hash of flags)
    my $i = 0;
    for my $item(@event) {
        my $revents = $item->{revents};
        my $out = $event->[$i++]->{revents} = { value => $revents };
        my $found = 0;  # can't mask off values, since there are dupes
        while (my ($name, $value) = each %Event) {
            $out->{$name} = 1, $found |= $value if $revents & $value;
        }
        $out->{unknown} = $revents & ~$found if $revents & ~$found;
    }
    $count
}

sub _cb_kbdint_response_default {
    my ($self, $user, $name, $instr, @prompt) = @_;
    require Term::ReadKey;

    local $| = 1;
    my $prompt = "[user $user] ";
    $prompt .= "$name\n" if $name;
    $prompt .= "$instr\n" if $instr;
    $prompt =~ s/ $/\n/;
    print $prompt;

    my @out;
    for my $prompt(@prompt) {
        print STDERR "$prompt->{text}";

        Term::ReadKey::ReadMode('noecho') unless $prompt->{echo};
        chomp(my $value = Term::ReadKey::ReadLine(0));
        Term::ReadKey::ReadMode('normal') unless $prompt->{echo};
        push @out, $value;
    }
    @out
}


# mechanics

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.

    my $constname;
    our $AUTOLOAD;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    croak "&Net::SSH2::constant not defined" if $constname eq 'constant';
    my ($error, $val) = constant($constname);
    if ($error) { croak $error; }
    {
        no strict 'refs';
        *$AUTOLOAD = sub { $val };
    }
    goto &$AUTOLOAD;
}

require XSLoader;
XSLoader::load('Net::SSH2', $VERSION);

_init_poll();

require Net::SSH2::Channel;
require Net::SSH2::SFTP;
require Net::SSH2::File;
require Net::SSH2::Listener;

1;
__END__

=head1 NAME

Net::SSH2 - Support for the SSH 2 protocol via libssh2.

=head1 SYNOPSIS

  use Net::SSH2;

  my $ssh2 = Net::SSH2->new();

  $ssh2->connect('example.com') or die $!;

  if ($ssh2->auth_keyboard('fizban')) {
      my $chan = $ssh2->channel();
      $chan->exec('program');

      my $sftp = $ssh2->sftp();
      my $fh = $sftp->open('/etc/passwd') or die;
      print $_ while <$fh>;
  }

=head1 DESCRIPTION

C<Net::SSH2> is a perl interface to the libssh2 (L<http://www.libssh2.org>)
library.  It supports the SSH2 protocol (there is no support for SSH1)
with all of the key exchanges, ciphers, and compression of libssh2.

Unless otherwise indicated, methods return a true value on success and
false on failure; use the error method to get extended error information.

The typical order is to create the SSH2 object, set up the connection methods
you want to use, call connect, authenticate with one of the C<auth> methods,
then create channels on the connection to perform commands.

=head1 EXPORTS

Exports the following constant tags:

=over 4

=item all

All constants.

=back

ssh constants:

=over 4

=item callback

=item channel

=item error

=item socket

=item trace

Tracing constants for use with C<< ->trace >> and C<< ->new(trace => ...) >>.

=item hash

Key hash constants.

=item method

=item disconnect

Disconnect type constants.

=back

SFTP constants:

=over 4

=item fx

=item fxf

=item sftp

=back

=head1 METHODS

=head2 new

Create new SSH2 object.

To turn on tracing with a debug build of libssh2 use:

    my $ssh2 = Net::SSH2->new(trace => -1);

=head2 banner ( text )

Set the SSH2 banner text sent to the remote host (prepends required "SSH-2.0-").

=head2 version

In scalar context, returns libssh2 version/patch e.g. 0.18 or "0.18.0-20071110".
In list context, returns that version plus the numeric version (major, minor,
and patch, each encoded as 8 bits, e.g. 0x001200 for version 0.18) and the
default banner text (e.g. "SSH-2.0-libssh2_0.18.0-20071110").

=head2 error

Returns the last error code; returns false if no error.  In list context,
returns (code, error name, error string).

=head2 sock

Returns a reference to the underlying L<IO::Socket::INET> object, or C<undef> if
not yet connected.

=head2 trace

Calls libssh2_trace with supplied bitmask, to enable all tracing use:

    $ssh2->trace(-1);

You need a debug build of libssh2 with tracing support.

=head2 method ( type [, values... ] )

Sets or returns a method preference; for get, pass in the type only; to set,
pass in either a list of values or a comma-separated string.  Values can only
be queried after the session is connected.

The following methods can be set or queried:

=over 4

=item KEX

Key exchange method names. Supported values:

=over 4

=item diffie-hellman-group1-sha1

Diffie-Hellman key exchange with SHA-1 as hash, and Oakley Group 2 (see RFC
2409).

=item diffie-hellman-group14-sha1

Diffie-Hellman key exchange with SHA-1 as hash, and Oakley Group 14 (see RFC
3526).

=item diffie-hellman-group-exchange-sha1

Diffie-Hellman key exchange with SHA-1 as hash, using a safe-prime/generator
pair (chosen by server) of arbitrary strength (specified by client) (see IETF
draft secsh-dh-group-exchange).

=back

=item HOSTKEY

Public key algorithms. Supported values:

=over 4

=item ssh-dss

Based on the Digital Signature Standard (FIPS-186-2).

=item ssh-rsa

Based on PKCS#1 (RFC 3447).

=back

=item CRYPT_CS

Encryption algorithm from client to server. Supported algorithms:

=over 4

=item aes256-cbc

AES in CBC mode, with 256-bit key.

=item rijndael-cbc@lysator.liu.se

Alias for aes256-cbc.

=item aes192-cbc

AES in CBC mode, with 192-bit key.

=item aes128-cbc

AES in CBC mode, with 128-bit key.

=item blowfish-cbc

Blowfish in CBC mode.

=item arcfour

ARCFOUR stream cipher.

=item cast128-cbc

CAST-128 in CBC mode.

=item 3des-cbc

Three-key 3DES in CBC mode.

=item none

No encryption.

=back

=item CRYPT_SC

Encryption algorithm from server to client. See L<CRYPT_CS> for supported
algorithms.

=item MAC_CS

Message Authentication Code (MAC) algorithms from client to server. Supported
values:

=over 4

=item hmac-sha1

SHA-1 with 20-byte digest and key length.

=item hmac-sha1-96

SHA-1 with 20-byte key length and 12-byte digest length.

=item hmac-md5

MD5 with 16-byte digest and key length.

=item hmac-md5-96

MD5 with 16-byte key length and 12-byte digest length.

=item hmac-ripemd160

RIPEMD-160 algorithm with 20-byte digest length.

=item hmac-ripemd160@openssh.com

Alias for hmac-ripemd160.

=item none

No encryption.

=back

=item MAC_SC

Message Authentication Code (MAC) algorithms from server to client. See
L<MAC_SC> for supported algorithms.

=item COMP_CS

Compression methods from client to server. Supported values:

=over 4

=item zlib

The "zlib" compression method as described in RFC 1950 and RFC 1951.

=item none

No compression

=back

=item COMP_SC

Compression methods from server to client. See L<COMP_CS> for supported
compression methods.

=back

=head2 connect ( handle | host [, port [, Timeout => secs ]] )

Accepts a handle over which to conduct the SSH 2 protocol.  The handle may be:

=over 4

=item an C<IO::*> object

=item a glob reference

=item an integer file descriptor

=item a host name and port

=back

=head2 disconnect ( [description [, reason [, language]]] )

Send a clean disconnect message to the remote server.  Default values are empty
strings for description and language, and C<SSH_DISCONNECT_BY_APPLICATION> for
the reason.

=head2 hostkey ( hash type )

Returns a hash of the host key; note that the key is raw data and may contain
nulls or control characters.  The type may be:

=over 4

=item MD5 (16 bytes)

=item SHA1 (20 bytes)

=back

=head2 auth_list ( [username] )

Get a list (or comma-separated string in scalar context) of authentication
methods supported by the server; or returns C<undef>.  If C<undef> is returned
and L<auth_ok> is true, the server accepted an unauthenticated session for the
given username.

=head2 auth_ok

Returns true iff the session is authenticated.

=head2 auth_password ( username [, password [, callback ]] )

Authenticate using a password (PasswordAuthentication must be enabled in
sshd_config or equivalent for this to work.)

If the password has expired, if a callback code reference was given, it's
called as C<callback($self, $username)> and should return a password.  If
no callback is provided, LIBSSH2_ERROR_PASSWORD_EXPIRED is returned.

=head2 auth_publickey ( username, public key, private key [, password ] )

Note that public key and private key are names of files containing the keys!

Authenticate using keys and an optional password.

=head2 auth_hostbased ( username, public key, private key, hostname,
 [, local username [, password ]] )

Host-based authentication using an optional password.  The local username
defaults to be the same as the remote username.

=head2 auth_keyboard ( username, password | callback )

Authenticate using "keyboard-interactive".  Takes either a password, or a
callback code reference which is invoked as C<callback-E<gt>(self, username,
name, instruction, prompt...)> (where each prompt is a hash with C<text> and
C<echo> keys, signifying the prompt text and whether the user input should be
echoed, respectively) which should return an array of responses.

If only a username is provided, the default callback will handle standard
interactive responses; L<Term::ReadKey> is required.

=head2 auth_agent ( username )

Try to authenticate using ssh-agent. This requires libssh2 version 1.2.3 or
later.

=head2 auth ( ... )

This is a general, prioritizing authentication mechanism that can use any
of the previous methods.  You provide it some parameters and (optionally)
a ranked list of methods you want considered (defaults to all).  It will
remove any unsupported methods or methods for which it doesn't have parameters
(e.g. if you don't give it a public key, it can't use publickey or hostkey),
and try the rest, returning whichever one succeeded or a false value if they
all failed.  If a parameter is passed with an undef value, a default value
will be supplied if possible.  The parameters are:

=over 4

=item rank

An optional ranked list of methods to try.  The names should be the names of
the L<Net::SSH2> C<auth> methods, e.g. 'keyboard' or 'publickey', with the
addition of 'keyboard-auto' for automated 'keyboard-interactive'.

=item username

=item password

=item publickey

=item privatekey

As in the methods, publickey and privatekey are filenames.

=item hostname

=item local_username

=item interact

If this is set to a true value, interactive methods will be considered.

=item cb_keyboard

L<auth_keyboard> callback.

=item cb_password

L<auth_password> callback.

=back

=head2 channel ( [type, [window size, [packet size]]] )

Creates and returns a new channel object.  The default type is "session".
See L<Net::SSH2::Channel>.

=head2 tcpip ( host, port [, shost, sport ] )

Creates a TCP connection from the remote host to the given host:port, returning
a new channel.  Binds to shost:sport (default 127.0.0.1:22).

=head2 listen ( port [, host [, bound port [, queue size ]]] )

Sets up a TCP listening port on the remote host.  Host defaults to 0.0.0.0;
if bound port is provided, it should be a scalar reference in which the bound
port is returned.  Queue size specifies the maximum number of queued connections
allowed before the server refuses new connections.

Returns a new Net::SSH2::Listener object.

=head2 scp_get ( remote [, local ] )

Retrieve a file with scp; local path defaults to basename of remote.  C<local>
may be an IO object (e.g. IO::File, IO::Scalar).

=head2 scp_put ( local [, remote ] )

Send a file with scp; remote path defaults to same as local.  C<local> may be
an IO object instead of a filename (but it must have a valid stat method).

=head2 sftp

Return SecureFTP interface object (see L<Net::SSH2::SFTP>).

=head2 public_key

Return public key interface object (see L<Net::SSH2::PublicKey>).

=head2 poll ( timeout, arrayref of hashes )

Pass in a timeout in milliseconds and an arrayref of hashes with the following
keys:

=over 4

=item handle

May be a L<Net::SSH2::Channel> or L<Net::SSH2::Listener> object, integer file
descriptor, or perl file handle.

=item events

Requested events.  Combination of LIBSSH2_POLLFD_* constants (with the POLL
prefix stripped if present), or an arrayref of the names ('in', 'hup' etc.).

=item revents

Returned events.  Returns a hash with the (lowercased) names of the received
events ('in', 'hup', etc.) as keys with true values, and a C<value> key with
the integer value.

=back

Returns undef on error, or the number of active objects.

=head2 block_directions

Get the blocked direction when a function returns LIBSSH2_ERROR_EAGAIN, returns
LIBSSH2_SOCKET_BLOCK_INBOUND or LIBSSH2_SOCKET_BLOCK_OUTBOUND from the socket
export group.

=head2 debug ( state )

Class method (affects all Net::SSH2 objects).  Pass 1 to enable, 0 to disable.
Debug output is sent to stderr via C<warn>.

=head2 blocking ( flag )

Enable or disable blocking.  Note that if blocking is disabled, methods that
create channels may fail, e.g. C<channel>, C<SFTP>, C<scp_*>.

=head1 SEE ALSO

L<Net::SSH2::Channel>, L<Net::SSH2::Listener>,
L<Net::SSH2::SFTP>, L<Net::SSH2::File>, L<Net::SSH2::Dir>.

LibSSH2 documentation at L<http://www.libssh2.org>.

IETF Secure Shell (secsh) working group at
L<http://www.ietf.org/html.charters/secsh-charter.html>.

L<Net::SSH::Perl>.

=head1 AUTHOR

David B. Robins, E<lt>dbrobins@cpan.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2005 - 2010 by David B. Robins; all rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.0 or,
at your option, any later version of Perl 5 you may have available.

=cut
