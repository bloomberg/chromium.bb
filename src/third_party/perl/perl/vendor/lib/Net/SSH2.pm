package Net::SSH2;

our $VERSION = '0.70';

use 5.006;
use strict;
use warnings;
use warnings::register;
use Carp;

require Net::SSH2::Constants;

use Socket;
use IO::File;
use File::Basename;
use Errno;

# load IO::Socket::IP when available, otherwise fallback to IO::Socket::INET.

my $socket_class = do {
    local ($SIG{__DIE__}, $SIG{__WARN__}, $@, $!);
    eval {
        require IO::Socket::IP;
        'IO::Socket::IP';
    }
} || do {
    require IO::Socket::INET;
    'IO::Socket::INET'
};

# methods

sub new {
    my ($class, %opts) = @_;
    my $self = $class->_new;

    for (qw(trace timeout debug)) {
        $self->$_($opts{$_}) if defined $opts{$_}
    }
    $self->flag(COMPRESS => $opts{compress})
        if defined $opts{compress} and (version())[1] >= 0x10500;
    $self->flag(SIGPIPE => $opts{sigpipe})
        if defined $opts{sigpipe};

    return $self;
}

sub die_with_error {
    my $self = shift;
    if (my ($code, $name, $string) = $self->error) {
        croak join(": ", @_, "$string ($code $name)");
    }
    else {
        croak join(": ", @_, "no libssh2 error registered");
    }
}

sub method {
    my $self = shift;
    my $method_type = shift;
    $self->_method($method_type => (@_ ? join(',', @_) : ()));
}

my $connect_opts_warned;
my $connect_fd_warned;
my $connect_void_warned;
sub connect {
    my $self = shift;
    defined $_[0] or croak "Net::SSH2::connect: hostname argument is undefined";

    # try to connect, or get a file descriptor
    my ($fd, $sock);
    if (@_ == 1) {
        $sock = shift;
        if ($sock =~ /^\d{1,10}$/) {
            $connect_fd_warned++ or
                warnings::warnif($self, "Passing a file descriptor number to connect is deprecated");
            $fd = $sock;
        } elsif(ref $sock) {
            # handled below
        } else {
            @_ = ($sock, 'ssh');
        }
    }

    my %opts = (@_ > 2 ? splice(@_, 2) : ());
    if (%opts) {
        $connect_opts_warned++ or
            warnings::warnif($self, "Passing options to connect is deprecated");
        $self->timeout(1000 * $opts{Timeout}) if $opts{Timeout};
        if ($opts{Compress} and
            ($self->version)[1] >= 0x10500) {
            $self->flag(COMPRESS => 1);
        }
    }

    my ($hostname, $port);
    if (@_ == 2) {
        ($hostname, $port) = @_;
        if (not defined $port) {
            $port = getservbyname('ssh', 'tcp') || 22;
        }
        elsif ($port =~ /\D/) {
            $port = getservbyname($port, 'tcp');
            unless (defined $port) {
                $self->_set_error(LIBSSH2_ERROR_SOCKET_NONE(), "Unable to resolve TCP service name $_[1]");
                goto error;
            }
        }

        my $timeout = $self->timeout;
        $sock = $socket_class->new( PeerHost => $hostname,
                                    PeerPort => $port,
                                    Blocking => $self->blocking,
                                    (defined($timeout) ? (Timeout => 0.001 * $timeout) : ()) );
        unless ($sock) {
            $self->_set_error(LIBSSH2_ERROR_SOCKET_NONE(), "Unable to connect to remote host: $!");
            goto error;
        }
        $sock->sockopt(SO_LINGER, pack('SS', 0, 0));
    }

    # get a file descriptor
    unless (defined $fd) {
        $fd = fileno($sock);
        unless (defined $fd) {
            $self->_set_error(LIBSSH2_ERROR_SOCKET_NONE(), "Unable to get file descriptor from socket: $!");
            goto error;
        }
    }

    if ($^O eq 'MSWin32') {
        require Win32API::File;
        $fd = Win32API::File::FdGetOsFHandle($fd);
    }

    {
        local ($@, $SIG{__DIE__});
        $port = eval { $sock->peerport } || 22
            unless defined $port;
        $hostname = eval { $sock->peername }
            unless defined $hostname;
    }

    # pass it in, do protocol
    return $self->_startup($fd, $sock, $hostname, $port);

 error:
    unless (defined wantarray) {
        unless ($connect_void_warned++) {
            local $!;
            warnings::warnif($self, "Calling connect in void context is deprecated");
        }
        croak "Net::SSH2: failed to connect to ". join(':', grep defined, @_[0,1]) .": $!";
    }
    return;
}

sub _auth_methods {
    return {
        'agent' => {
            ssh => 'publickey',
            method => \&auth_agent,
            params => [qw(_fallback username)],
        },
        'hostbased'     => {
            ssh    => 'hostbased',
            method => \&auth_hostbased,
            params => [qw(username publickey privatekey
                       hostname local_username? passphrase?)],
        },
        'publickey'     => {
            ssh    => 'publickey',
            method => \&auth_publickey,
            params => [qw(username publickey? privatekey passphrase?)],
        },
        'keyboard'      => {
            ssh    => 'keyboard-interactive',
            method => \&auth_keyboard,
            params => [qw(_interact _fallback username cb_keyboard?)]
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
        'password-interact'  => {
             ssh    => 'password',
             method => \&auth_password_interact,
             params => [qw(_interact _fallback username cb_password?)],
        },
        'none'          => {
            ssh    => 'none',
            method => \&auth_password,
            params => [qw(username)],
        },
    };
}

my @rank_default = qw(hostbased publickey keyboard-auto password agent keyboard password-interact none);

sub _auth_rank {
    my ($self, $rank) = @_;
    $rank ||= \@rank_default;
    my $libver = ($self->version)[1] || 0;
    return @$rank if $libver > 0x010203;
    return grep { $_ ne 'agent' } @$rank;
}

sub _local_user {
    for (qw(USER LOGNAME)) {
        return $ENV{$_} if defined $ENV{$_}
    }

    local ($@, $SIG{__DIE__}, $SIG{__WARN__});

    my $u = eval { getlogin };
    return $u if defined $u;

    eval { getpwuid $< }
}

my $password_when_you_mean_passphrase_warned;
sub auth {
    my ($self, %p) = @_;

    $self->_set_error(LIBSSH2_ERROR_AUTHENTICATION_FAILED(),
                      "Authentication failed"); # default error

    $p{username} = _local_user unless defined $p{username};

    my @rank = $self->_auth_rank(delete $p{rank});
    my $remote_rank;
    $remote_rank = { map { $_ => 1 } $self->auth_list($p{username}) }
        if defined $p{username};

    # if fallback is set, interact with the user even when a password
    # is given
    $p{fallback} = 1 unless defined $p{password} or defined $p{passphrase};

    TYPE: for(my $i = 0; $i < @rank; $i++) {
        my $type = $rank[$i];
        my $data = $self->_auth_methods->{$type};
        unless ($data) {
            carp "unknown authentication method '$type'";
            next;
        }
        next if $remote_rank and !$remote_rank->{$data->{ssh}};

        # do we have the required parameters?
        my @pass;
        for my $param(@{$data->{params}}) {
            my $p = $param;
            my $opt = $p =~ s/\?$//;
            my $pseudo = $p =~ s/^_//;

            if ($p eq 'passphrase' and not exists $p{$p} and defined $p{password}) {
                $p = 'password';
                $password_when_you_mean_passphrase_warned++
                    or carp "Using the key 'password' to refer to a passphrase is deprecated. Use 'passphrase' instead";
            }

            if ($pseudo) {
                next TYPE unless $p{$p};
            }
            else {
                next TYPE unless $opt or defined $p{$p};
                push @pass, $p{$p};  # if it's optional, store undef
            }
        }

        # invoke the authentication method
        return $type if $data->{method}->($self, @pass) and $self->auth_ok;
    }

    return 'none' if  $self->auth_ok;
    return;  # failure
}

my $term_readkey_unavailable_warned;
my $term_readkey_loaded;
sub _load_term_readkey {
    return 1 if $term_readkey_loaded ||= do {
        local ($@, $!, $SIG{__DIE__}, $SIG{__WARN__});
        eval { require Term::ReadKey; 1 }
    };

    carp "Unable to load Term::ReadKey, will not ask for passwords at the console!"
        unless $term_readkey_unavailable_warned++;
    return;
}

sub _print_stderr {
    my $self = shift;
    my $ofh = select STDERR; local $|= 1; select $ofh;
    print STDERR $_ for @_;
}

sub _ask_user {
    my ($self, $prompt, $echo) = @_;
    my $timeout;
    if (($self->version)[1] >= 0x10209) {
        $timeout = $self->timeout || 0;
        $timeout = ($timeout + 999) / 1000;
    }
    _load_term_readkey or return;
    $self->_print_stderr($prompt);
    Term::ReadKey::ReadMode('noecho') unless $echo;
    my $reply = Term::ReadKey::ReadLine($timeout);
    Term::ReadKey::ReadMode('normal') unless $echo;
    $self->_print_stderr("\n")
        unless $echo and defined $reply;
    if (defined $reply) {
        chomp $reply
    }
    else {
        $self->_set_error(LIBSSH2_ERROR_SOCKET_TIMEOUT(),
                          "Timeout waiting for user response!");
    }
    return $reply;
}

sub auth_password_interact {
    my ($self, $username, $cb) = @_;
    _load_term_readkey or return;
    my $rc;
    for (0..2) {
        my $password = $self->_ask_user("${username}'s password? ", 0);
        $rc = $self->auth_password($username, $password, $cb);
        last if $rc or $self->error != LIBSSH2_ERROR_AUTHENTICATION_FAILED();
        my $ofh = select STDERR; local $|= 1; select $ofh;
        $self->_print_stderr("Password authentication failed!\n");
    }
    return $rc;
}

sub _local_home {
    return $ENV{HOME} if defined $ENV{HOME};
    local ($@, $SIG{__DIE__}, $SIG{__WARN__});
    my $home = eval { (getpwuid($<))[7] };
    return $home;
}

my $check_hostkey_void_ctx_warned;
sub check_hostkey {
    my ($self, $policy, $path, $comment) = @_;

    defined wantarray or $check_hostkey_void_ctx_warned++ or
        warnings::warnif($self, "Calling check_hostkey in void context is useless");

    my $cb;
    if (not defined $policy) {
        $policy = LIBSSH2_HOSTKEY_POLICY_STRICT();
    }
    elsif (ref $policy eq 'CODE') {
        $cb = $policy;
    }
    else {
        $policy =  _parse_constant(HOSTKEY_POLICY => $policy);
    }

    my $hostname = $self->hostname;
    croak("hostname unknown: in order to use check_hostkey the peer host name ".
          "must be given (or discoverable) at connect time")
        unless defined $hostname;

    unless (defined $path) {
        my $home = _local_home;
        unless (defined $home) {
            $self->_set_error(LIBSSH2_ERROR_FILE(), "Unable to determine known_hosts location");
            return;
        }
        require File::Spec;
        $path = File::Spec->catfile($home, '.ssh', 'known_hosts');
    }

    my ($check, $key, $type, $flags);
    my $kh = $self->known_hosts;
    if ($kh and defined $kh->readfile($path)) {

        ($key, $type) = $self->remote_hostkey;
        $flags = ( LIBSSH2_KNOWNHOST_TYPE_PLAIN() |
                   LIBSSH2_KNOWNHOST_KEYENC_RAW() |
                   (($type + 1) << LIBSSH2_KNOWNHOST_KEY_SHIFT()) );

        $check = $kh->check($hostname, $self->port, $key, $flags);
        $check == LIBSSH2_KNOWNHOST_CHECK_MATCH() and return "00";
    }
    else {
        $check = LIBSSH2_KNOWNHOST_CHECK_FAILURE();
    }

    if ($cb) {
        my $ok = $cb->($self, $check, $comment);
        $ok or $self->_set_error(LIBSSH2_ERROR_KNOWN_HOSTS(), 'Host key verification failed');
        return $ok;
    }

    return $check
        if $policy == LIBSSH2_HOSTKEY_POLICY_ADVISORY(); # user doesn't care!

    if ($check == LIBSSH2_KNOWNHOST_CHECK_NOTFOUND()) {
        $self->_set_error(LIBSSH2_ERROR_KNOWN_HOSTS(), 'Unable to verify host key, host not found');
        unless ($policy == LIBSSH2_HOSTKEY_POLICY_TOFU()) {
            if ($policy == LIBSSH2_HOSTKEY_POLICY_ASK()) {
                my $fp = unpack 'H*', $self->hostkey_hash(LIBSSH2_HOSTKEY_HASH_SHA1());
                my $yes = $self->_ask_user("The authenticity of host '$hostname' can't be established.\n" .
                                           "Key fingerprint is SHA1:$fp.\n" .
                                           "Are you sure you want to continue connecting (yes/no)? ", 1);
                unless ($yes =~ /^y(es)?$/i) {
                    $self->_set_error(LIBSSH2_ERROR_KNOWN_HOSTS(), 'Host key verification failed: user did not accept the key');
                    return undef;
                }
            }
        }

        $comment = '(Net::SSH2)' unless defined $comment;
        # we ignore errors here, that is the usual SSH client behaviour
        $kh->add($hostname, $self->port, $key, $comment, $flags) and
            $kh->writefile($path);

        return $check;
    }

    $self->_set_error(LIBSSH2_ERROR_KNOWN_HOSTS(), 'Host key verification failed: '.
                      ( ($check == LIBSSH2_KNOWNHOST_CHECK_NOTFOUND())
                        ? "key not found in '$path'"
                        : "unable to perform the check"));
    return undef;
}

sub scp_get {
    my ($self, $remote, $path) = @_;
    $path = basename $remote if not defined $path;

    my %stat;
    $self->blocking(1);
    my $chan = $self->_scp_get($remote, \%stat) or return;

    # read and commit blocks until we're finished
    my $file;
    if (ref $path) {
        $file = $path;
    }
    else {
        my $mode = $stat{mode} & 0777;
        $file = IO::File->new($path, O_WRONLY | O_CREAT | O_TRUNC, $mode);
        unless ($file) {
            $self->_set_error(LIBSSH2_ERROR_FILE(), "Unable to open local file: $!");
            return;
        }
        binmode $file;
    }

    my $size = $stat{size};
    while ($size > 0) {
        my $bytes_read = $chan->read(my($buf), (($size > 40000 ? 40000 : $size)));
        if ($bytes_read) {
            $size -= $bytes_read;
            while (length $buf) {
                my $bytes_written = $file->syswrite($buf, length $buf);
                if ($bytes_written) {
                    substr $buf, 0, $bytes_written, '';
                }
                elsif ($! != Errno::EAGAIN() &&
                       $! != Errno::EINTR()) {
                    $self->_set_error(LIBSSH2_ERROR_FILE(), "Unable to write to local file: $!");
                    return;
                }
            }
        }
        elsif (!defined($bytes_read) and
               $self->error != LIBSSH2_ERROR_EAGAIN()) {
            return;
        }
    }

    # process SCP acknowledgment and send same
    $chan->read(my $eof, 1);
    $chan->write("\0");
    return 1;
}

sub scp_put {
    my ($self, $path, $remote) = @_;
    $remote = basename $path if not defined $remote;

    my $file;
    if (ref $path) {
        $file = $path;
    }
    else {
        $file = IO::File->new($path, O_RDONLY);
        unless ($file) {
            $self->_set_error(LIBSSH2_ERROR_FILE(), "Unable to open local file: $!");
            return;
        }
        binmode $file;
    }

    my @stat = $file->stat;
    unless (@stat) {
        $self->_set_error(LIBSSH2_ERROR_FILE(), "Unable to stat local file: $!");
        return;
    }

    my $mode = $stat[2] & 0777;  # mask off extras such as S_IFREG
    $self->blocking(1);
    my $chan = $self->_scp_put($remote, $mode, @stat[7, 8, 9]) or return;

    # read and transmit blocks until we're finished
    my $size = $stat[7];
    while ($size > 0) {
        my $bytes_read = $file->sysread(my($buf), ($size > 32768 ? 32768 : $size));
        if ($bytes_read) {
            $size -= $bytes_read;
            while (length $buf) {
                my $bytes_written = $chan->write($buf);
                if (defined $bytes_written) {
                    substr($buf, 0, $bytes_written, '');
                }
                elsif ($chan->error != LIBSSH2_ERROR_EAGAIN()) {
                    return;
                }
            }
        }
        elsif (defined $bytes_read) {
            $self->_set_error(LIBSSH2_ERROR_FILE(), "Unexpected end of local file");
            return;
        }
        elsif ($! != Errno::EAGAIN() and
               $! != Errno::EINTR()) {
            $self->_set_error(LIBSSH2_ERROR_FILE(), "Unable to read local file: $!");
            return;
        }
    }

    # send/receive SCP acknowledgement
    $chan->write("\0");
    return $chan->read(my($eof), 1) || undef;
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
    _load_term_readkey or return;

    my $prompt = "[user $user] ";
    $prompt .= "$name\n" if $name;
    $prompt .= "$instr\n" if $instr;
    $prompt =~ s/ $/\n/;
    $self->_print_stderr($prompt);

    return map $self->_ask_user($_->{text}, $_->{echo}), @prompt;
}

my $hostkey_warned;
sub hostkey {
    $hostkey_warned++ or carp "Net::SSH2 'hostkey' method is obsolete, use 'hostkey_hash' instead";
    shift->hostkey_hash(@_);
}

sub auth_list {
    my $auth = shift->_auth_list(@_);
    return unless defined $auth;
    wantarray ? split(/,/, $auth) : $auth
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
require Net::SSH2::KnownHosts;

1;
__END__

=head1 NAME

Net::SSH2 - Support for the SSH 2 protocol via libssh2.

=head1 SYNOPSIS

  use Net::SSH2;

  my $ssh2 = Net::SSH2->new();

  $ssh2->connect('example.com') or $ssh2->die_with_error;

  $ssh->check_hostkey('ask') or $ssh2->die_with_error;

  if ($ssh2->auth_keyboard('fizban')) {
      my $chan = $ssh2->channel();
      $chan->exec('program');

      my $sftp = $ssh2->sftp();
      my $fh = $sftp->open('/etc/passwd') or $sftp->die_with_error;
      print $_ while <$fh>;
  }

=head1 DESCRIPTION

Net::SSH2 is a Perl interface to the libssh2
(L<http://www.libssh2.org>) library.  It supports the SSH2 protocol
(there is no support for SSH1) with all of the key exchanges, ciphers,
and compression of libssh2.

Even if the module can be compiled and linked against very old
versions of the library, nothing below 1.5.0 should really be used
(older versions were quite buggy and unreliable) and version 1.7.0 or
later is recommended.

=head2 Error handling

Unless otherwise indicated, methods return a true value on success and
C<undef> on failure; use the L</error> method to get extended error
information.

B<Important>: methods in Net::SSH2 not backed by libssh2 functions
(i.e. L</check_hostkey> or L<SCP|/scp_get> related methods) require
libssh2 1.7.0 or later in order to set the error state. That means
that after any of those methods fails, L</error> would not return the
real code but just some bogus result when an older version of the
library is used.

=head2 Typical usage

The typical usage order is as follows:

=over 4

=item 1

Create the SSH2 object calling L</new>.

=item 2

Configure the session if required. For instance, enabling compression
or picking some specific encryption methods.

=item 3

Establish the SSH connection calling the method L</connect>.

=item 4

Check the remote host public key calling L</check_hostkey>.

=item 5

Authenticate calling the required L<authentication methods|/auth>.

=item 6

Call L</channel> and related methods to create new bidirectional
communication channels over the SSH connection.

=item 7

Close the connection letting the Net::SSH2 object go out of scope or
calling L</disconnect> explicitly.

=back

=head1 CONSTANTS

All the constants defined in libssh2 can be imported from
Net::SSH2.

For instance:

   use Net::SSH2 qw(LIBSSH2_CHANNEL_EXTENDED_DATA_MERGE
                    LIBSSH2_CHANNEL_FLUSH_ALL
                    LIBSSH2_HOSTKEY_POLICY_ASK);

Though note that most methods accept the uncommon part of the
constant name as a string. For instance the following two method calls
are equivalent:

    $channel->ext_data(LIBSSH2_CHANNEL_EXTENDED_DATA_MERGE);
    $channel->ext_data('merge');

Tags can be used to import the following constant subsets:

  callback channel error socket trace hash method
  disconnect policy fx fxf sftp

The tag C<all> can also be used to import all of them.

=head1 METHODS

=head2 new ( %options )

Create new Net::SSH2 object representing a SSH session.

The accepted options are as follows:

=over 4

=item timeout

Sets the default timeout in milliseconds. See L</timeout>.

=item trace

Sets tracing. See L</trace>.

Example:

    my $ssh2 = Net::SSH2->new(trace => -1);

Note that tracing requires a version of libssh2 compiled with debugging support.

=item debug

Enable debugging. See L</debug>.

=item compress

Sets flag C<LIBSSH2_FLAG_COMPRESS>. See L</flag>.

=item sigpipe

Sets flag C<LIBSSH2_FLAG_SIGPIPE>. See L</flag>.

=back

=head2 banner ( text )

Set the SSH2 banner text sent to the remote host (prepends required "SSH-2.0-").

=head2 version

In scalar context, returns libssh2 version/patch e.g. 0.18 or "0.18.0-20071110".
In list context, returns that version plus the numeric version (major, minor,
and patch, each encoded as 8 bits, e.g. 0x001200 for version 0.18) and the
default banner text (e.g. "SSH-2.0-libssh2_0.18.0-20071110").

=head2 error

Returns the last error code. In list context,
returns (code, error name, error string).

Note that the returned error value is only meaningful after some other
method indicates an error by returning false.

=head2 die_with_error ( [message] )

Calls C<die> with the given message and the error information from the
object appended.

For instance:

  $ssh2->connect("ajhkfhdklfjhklsjhd", 22)
      or $ssh2->die_with_error;
  # dies as:
  #    Unable to connect to remote host: Invalid argument (-1 LIBSSH2_ERROR_SOCKET_NONE)

=head2 sock

Returns a reference to the underlying L<IO::Socket> object (usually a
derived class as L<IO::Socket::IP> or L<IO::Socket::INET>), or
C<undef> if not yet connected.

=head2 trace

Calls C<libssh2_trace> with supplied bitmask. In order to enable all
tracing pass C<-1> as follows:

    $ssh2->trace(-1);

A version of libssh2 compiled with tracing support is required.

=head2 timeout ( timeout_ms )

Enables a global timeout (in milliseconds) which will affect every
action (requires libssh2 1.2.9 or later).

By default, or if you set the timeout to zero, Net::SSH2 has no
timeout.

Note that timeout errors may leave the SSH connection in an
inconsistent state and further operations may fail or behave
incorrectly. Actually, some methods are able to recover after a
timeout error and others are not.

I<Don't hesitate to report any issue you encounter related to this so
that it can be fixed or at least, documented!>

=head2 method ( type [, values... ] )

Sets or gets a method preference. For get, pass in the type only; to
set, pass in either a list of values or a comma-separated
string. Values can only be queried after the session is connected.

The following methods can be set or queried:

=over 4

=item LIBSSH2_METHOD_KEX

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

=item LIBSSH2_METHOD_HOSTKEY

Public key algorithms. Supported values:

=over 4

=item ssh-dss

Based on the Digital Signature Standard (FIPS-186-2).

=item ssh-rsa

Based on PKCS#1 (RFC 3447).

=back

=item LIBSSH2_METHOD_CRYPT_CS

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

=item LIBSSH2_METHOD_CRYPT_SC

Encryption algorithm from server to client. See the
C<LIBSSH2_METHOD_CRYPT_CS> entry above for supported algorithms.

=item LIBSSH2_METHOD_MAC_CS

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

=item LIBSSH2_METHOD_MAC_SC

Message Authentication Code (MAC) algorithms from server to client. See
L<LIBSSH2_METHOD_MAC_CS> for supported algorithms.

=item LIBSSH2_METHOD_COMP_CS

Compression methods from client to server. Supported values:

=over 4

=item zlib

The "zlib" compression method as described in RFC 1950 and RFC 1951.

=item none

No compression

=back

=item LIBSSH2_METHOD_COMP_SC

Compression methods from server to client. See
L<LIBSSH2_METHOD_COMP_CS> for supported compression methods.

=back

=head2 connect ( handle | host [, port])

The argument combinations accepted are as follows:

=over 4

=item a glob or C<IO::*> object reference

Note that tied file handles are not acceptable. The underlying
libssh2 requires real file handles.

=item host [, port]

In order to handle IPv6 addresses the optional module
L<IO::Socket::IP> is required.

The port number defaults to 22.

=back

This method used to accept a C<Timeout> argument. That feature has
been replaced by the constructor C<timeout> option but note that it
takes milliseconds instead of seconds!

=head2 disconnect ( [description [, reason [, language]]] )

Sends a clean disconnect message to the remote server. Default values are empty
strings for description and language, and C<SSH_DISCONNECT_BY_APPLICATION> for
the reason.

=head2 hostname

The name of the remote host given at connect time or retrieved from
the TCP layer.

=head2 port

The port number of the remote SSH server.

=head2 hostkey_hash ( hash type )

Returns a hash of the host key; note that the key is raw data and may contain
nulls or control characters.

The type may be as follows:

=over 4

=item LIBSSH2_HOSTKEY_HASH_MD5

MD5 hash, 16 bytes long (requires libssh2 compiled with MD5 support).

=item LIBSSH2_HOSTKEY_HASH_SHA1

SHA1 hash, 20 bytes long.

=back

Note: in previous versions of the module this method was called
C<hostkey>.

=head2 remote_hostkey

Returns the public key from the remote host and its type which is one of
C<LIBSSH2_HOSTKEY_TYPE_RSA>, C<LIBSSH2_HOSTKEY_TYPE_DSS>, or
C<LIBSSH2_HOSTKEY_TYPE_UNKNOWN>.

=head2 check_hostkey( [policy, [known_hosts_path [, comment] ] ] )

Looks for the remote host key inside the given known host file
(defaults to C<~/.ssh/known_hosts>).

On success, this method returns the result of the call done under the
hood to C<Net::SSH2::KnownHost::check>
(i.e. C<LIBSSH2_KNOWNHOST_CHECK_MATCH>,
C<LIBSSH2_KNOWNHOST_CHECK_FAILURE>,
C<LIBSSH2_KNOWNHOST_CHECK_NOTFOUND> or
C<LIBSSH2_KNOWNHOST_CHECK_MISMATCH>).

On failure it returns C<undef>.

The accepted policies are as follows:

=over 4

=item LIBSSH2_HOSTKEY_POLICY_STRICT

Only host keys already present in the known hosts file are accepted.

This is the default policy.

=item LIBSSH2_HOSTKEY_POLICY_ASK

If the host key is not present in the known hosts file, the user is
asked if it should be accepted or not.

If accepted, the key is added to the known host file with the given
comment.

=item LIBSSH2_HOSTKEY_POLICY_TOFU

Trust On First Use: if the host key is not present in the known hosts
file, it is added there and accepted.

=item LIBSSH2_HOSTKEY_POLICY_ADVISORY

The key is always accepted, but it is never saved into the known host
file.

=item callback

If a reference to a subroutine is given, it is called when the key is
not present in the known hosts file or a different key is found. The
arguments passed to the callback are the session object, the matching
error (C<LIBSSH2_KNOWNHOST_CHECK_FAILURE>,
C<LIBSSH2_KNOWNHOST_CHECK_NOTFOUND> or
C<LIBSSH2_KNOWNHOST_CHECK_MISMATCH>) and the comment.

=back

=head2 auth_list ( [username] )

Returns the authentication methods accepted by the server. In scalar
context the methods are returned as a comma separated string.

When the server accepted an unauthenticated session for the given
username, this method returns C<undef> but L</auth_ok> returns true.

=head2 auth_ok

Returns true when the session is authenticated.

=head2 auth_password ( username [, password [, callback ]] )

Authenticates using a password.

If the password has expired, if a callback code reference was given, it's
called as C<callback($self, $username)> and should return a password.  If
no callback is provided, LIBSSH2_ERROR_PASSWORD_EXPIRED is returned.

=head2 auth_password_interact ( username [, callback])

Prompts the user for the password interactively (requires
L<Term::ReadKey>).

=head2 auth_publickey ( username, publickey_path, privatekey_path [, passphrase ] )

Authenticate using the given private key and an optional passphrase.

When libssh2 is compiled using OpenSSL as the crypto backend, passing
this method C<undef> as the public key argument is acceptable (OpenSSL
is able to extract the public key from the private one).

See also L<Supported key formats>.

=head2 auth_publickey_frommemory ( username, publickey_blob, privatekey_blob [, passphrase ] )

Authenticate using the given public/private key and an optional
passphrase. The keys must be PEM encoded (requires libssh2 1.6.0 or
later with the OpenSSL backend).

=head2 auth_hostbased ( username, publickey, privatekey, hostname,
 [, local username [, passphrase ]] )

Host-based authentication using an optional passphrase. The local username
defaults to be the same as the remote username.

=head2 auth_keyboard ( username, password | callback )

Authenticate using C<keyboard-interactive>. Takes either a password,
or a callback code reference which is invoked as
C<callback-E<gt>(self, username, name, instruction, prompt...)> (where
each prompt is a hash with C<text> and C<echo> keys, signifying the
prompt text and whether the user input should be echoed, respectively)
which should return an array of responses.

If only a username is provided, the default callback will handle standard
interactive responses (requires L<Term::ReadKey>)

=head2 auth_agent ( username )

Try to authenticate using an SSH agent (requires libssh2 1.2.3).

=head2 auth ( ... )

This is a general, prioritizing authentication mechanism that can use
any of the previous methods. You provide it some parameters and
(optionally) a ranked list of methods you want considered (defaults to
all). It will remove any unsupported methods or methods for which it
doesn't have parameters (e.g. if you don't give it a public key, it
can't use publickey or hostkey), and try the rest, returning whichever
one succeeded or C<undef> if they all failed. If a parameter is passed
with an C<undef> value, a default value will be supplied if possible.

The parameters are:

=over 4

=item rank

An optional ranked list of methods to try.  The names should be the
names of the L<Net::SSH2> C<auth> methods, e.g. C<keyboard> or
C<publickey>, with the addition of C<keyboard-auto> for automated
C<keyboard-interactive> and C<password-interact> which prompts the
user for the password interactively.

=item username

=item password

=item publickey

=item privatekey

C<privatekey> and C<publickey> are file paths.

=item passphrase

=item hostname

=item local_username

=item interact

If this option is set to a true value, interactive methods will be enabled.

=item fallback

If a password is given but authentication using it fails, the module
will fall back to ask the user for another password if this
parameter is set to a true value.

=item cb_keyboard

L<auth_keyboard> callback.

=item cb_password

L<auth_password> callback.

=back

For historical reasons and in order to maintain backward compatibility
with older versions of the module, when the C<password> argument is
given, it is also used as the passphrase (and a deprecation warning
generated).

In order to avoid that behaviour the C<passphrase> argument must be
also passed (it could be C<undef>). For instance:

  $ssh2->auth(username => $user,
              privatekey => $privatekey_path,
              publickey => $publickey_path,
              password => $password,
              passphrase => undef);

This work around will be removed in a not too distant future version
of the module.

=head2 flag (key, value)

Sets the given session flag.

The currently supported flag values are:

=over 4

=item LIBSSH2_FLAG_COMPRESS

If set before the connection negotiation is performed, compression
will be negotiated for this connection.

Compression can also be enabled passing option C<compress> to the
constructor L<new>.

=item LIBSSH2_FLAG_SIGPIPE

if set, Net::SSH2/libssh2 will not attempt to block SIGPIPEs but will
let them trigger from the underlying socket layer.

=back

=head2 keepalive_config(want_reply, interval)

Set how often keepalive messages should be sent.

C<want_reply> indicates whether the keepalive messages should request
a response from the server. C<interval> is number of seconds that can
pass without any I/O.

=head2 keepalive_send

Send a keepalive message if needed.

On failure returns undef. On success returns how many seconds you can
sleep after this call before you need to call it again.

Note that the underlying libssh2 function C<libssh2_keepalive_send>
can not recover from EAGAIN errors. If this method fails with such
error, the SSH connection may become corrupted.

The usage of this function is discouraged.

=head2 channel ( [type, [window size, [packet size]]] )

Creates and returns a new channel object. See L<Net::SSH2::Channel>.

Type, if given, must be C<session> (a reminiscence of an old, more
generic, but never working wrapping).

=head2 tcpip ( host, port [, shost, sport ] )

Creates a TCP connection from the remote host to the given host:port,
returning a new channel.

The C<shost> and C<sport> arguments are merely informative and passed
to the remote SSH server as the origin of the connection. They default
to 127.0.0.1:22.

Note that this method does B<not> open a new port on the local machine
and forwards incoming connections to the remote side.

=head2 listen ( port [, host [, bound port [, queue size ]]] )

Sets up a TCP listening port on the remote host.  Host defaults to 0.0.0.0;
if bound port is provided, it should be a scalar reference in which the bound
port is returned. Queue size specifies the maximum number of queued connections
allowed before the server refuses new connections.

Returns a new Net::SSH2::Listener object.

=head2 scp_get ( remote_path [, local_path ] )

Retrieve a file with SCP. Local path defaults to basename of remote.

Alternatively, C<local_path> may be an already open file handle or an
IO::Handle object (e.g. IO::File, IO::Scalar).

=head2 scp_put ( local_path [, remote_path ] )

Send a file with SCP. Remote path defaults to same as local.

Alternatively, C<local_path> may be an already open file handle or a
reference to a IO::Handle object (it must have a valid stat method).

=head2 sftp

Return SecureFTP interface object (see L<Net::SSH2::SFTP>).

Note that SFTP support in libssh2 is pretty rudimentary. You should
consider using L<Net::SFTP::Foreign> with the L<Net::SSH2> backend
L<Net::SFTP::Foreign::Backend::Net_SSH2> instead.

=head2 public_key

Return public key interface object (see L<Net::SSH2::PublicKey>).

=head2 known_hosts

Returns known hosts interface object (see L<Net::SSH2::KnownHosts>).

=head2 poll ( timeout, arrayref of hashes )

B<Deprecated>: the poll functionality in libssh2 is deprecated and
its usage disregarded. Session methods L</sock> and
L</block_directions> can be used instead to integrate Net::SSH2
inside an external event loop.

Pass in a timeout in milliseconds and an arrayref of hashes with the
following keys:

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

Get the blocked direction after some method returns
C<LIBSSH2_ERROR_EAGAIN>.

Returns C<LIBSSH2_SESSION_BLOCK_INBOUND> or/and
C<LIBSSH2_SESSION_BLOCK_OUTBOUND>.

=head2 debug ( state )

Class method (affects all Net::SSH2 objects).

Pass 1 to enable, 0 to disable. Debug output is sent to C<STDERR>.

=head2 blocking ( flag )

Enable or disable blocking.

A good number of the methods in C<Net::SSH2>/C<libssh2> can not work
in non-blocking mode. Some of them may just forcibly enable blocking
during its execution. A few may even corrupt the SSH session or crash
the program.

The ones that can be safely called are C<read> and, with some
caveats, C<write>. See L<Net::SSH2::Channel/write>.

I<Don't hesitate to report any bug you found in that area!>

=head1 INTEROPERABILITY AND OTHER KNOWN ISSUES

=head2 Protocol versions

The underlaying C<libssh2> library does support version 2 of the SSH
protocol exclusively (hopefully, version 1 usage is almost extinct).

The SFTP client implements version 3 of the SFTP protocol.

=head2 Key formats

Private and public keys can be generated and stored using different
formats and cyphers. Which ones are accepted by C<Net::SSH2> depends
on the libssh2 version being used and of the underlying crypto backend
(OpenSSL C<libssl> or C<libgcrypt>) it was
configured to use at build time.

An increassingly common problem is that OpenSSH since version 7.8
(released 2018-8-24) generates keys by default using the format
RFC4716 which is not supported by C<libssl>, the default crypto
backend.

Keys can be converted inplace to the old PEM format using
L<ssh-keygen(1)> as follows:

  $ ssh-keygen -p -m PEM -N "" -f ~/.ssh/id_rsa

On Windows, PuTTYgen (which is part of the PuTTY distribution) can be
used to convert keys.

Another common issue is that in the last years OpenSSH has
incorporated several new cyphers that are not supported by any version
of C<libssh2> yet (though the incomming 1.8.1 may aliviate the
situation). Currently the best option from an interoperability
standpoint is probably to stick to RSA key usage.

=head2 Security

Nowadays C<libssh2> development is not thrilling; new versions (even
minor ones) are being released just every two or three years. On the
other hand security issues are found and reported far more
frequently. That means that C<Net::SSH2>/C<libssh2> could be an easy
attack vector.

So, Net::SSH2 must be used with care only in trusted environments.

More specifically, using it to connect to untrusted third party
computers over the Internet may be a very bad idea!

=head1 SEE ALSO

L<Net::SSH2::Channel>, L<Net::SSH2::Listener>,
L<Net::SSH2::SFTP>, L<Net::SSH2::File>, L<Net::SSH2::Dir>.

LibSSH2 documentation at L<http://www.libssh2.org>.

IETF Secure Shell (secsh) working group at
L<http://www.ietf.org/html.charters/secsh-charter.html>.

L<Net::SSH::Any> and L<Net::SFTP::Foreign> integrate nicely with Net::SSH2.

Other Perl modules related to SSH you may find interesting:
L<Net::OpenSSH>, L<Net::SSH::Perl>, L<Net::OpenSSH::Parallel>,
L<Net::OpenSSH::Compat>.

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2005 - 2010 by David B. Robins (dbrobins@cpan.org).

Copyright (C) 2010 - 2016 by Rafael Kitover (rkitover@cpan.org).

Copyright (C) 2011 - 2019 by Salvador FandiE<ntilde>o (salva@cpan.org).

All rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.0 or,
at your option, any later version of Perl 5 you may have available.

=cut
