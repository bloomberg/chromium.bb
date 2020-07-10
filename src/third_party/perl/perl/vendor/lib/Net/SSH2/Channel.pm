package Net::SSH2::Channel;

use strict;
use warnings;
use Carp;

# methods

sub shell {
    $_[0]->process('shell')
}

sub exec {
    $_[0]->process(exec => $_[1])
}

sub subsystem {
    $_[0]->process(subsystem => $_[1])
}

sub error {
    shift->session->error(@_)
}

sub blocking {
    shift->session->blocking(@_)
}

sub setenv {
    my ($self, %env) = @_;
    my $rc = 1;
    while (my ($k, $v) = each %env) {
        $self->_setenv($k, $v)
            or undef $rc;
    }
    $rc
}

sub read1 {
    my $self = shift;
    my $buffer;
    my $rc = $self->read($buffer, @_);
    return (defined $rc ? $buffer : undef);
}

sub read2 {
    my ($self, $max_size) = @_;
    $max_size = 32678 unless defined $max_size;
    my $ssh2 = $self->session;
    my $old_blocking = $ssh2->blocking;
    my $timeout = $ssh2->timeout;
    my $delay = (($timeout and $timeout < 2000) ? 0.0005 * $timeout : 1);
    my $deadline;
    $deadline = time + 1 + 0.001 * $timeout if $timeout;
    $ssh2->blocking(0);
    while (1) {
        my @out;
        my $bytes;
        my $fail;
        my $zero;
        for (0, 1) {
            my $rc = $self->read($out[$_], $max_size, $_);
            if (defined $rc) {
                $rc or $zero++;
                $bytes += $rc;
                $deadline = time + 1 + 0.001 * $timeout if $timeout;
            }
            else {
                $out[$_] = '';
                if ($ssh2->error != Net::SSH2::LIBSSH2_ERROR_EAGAIN()) {
                    $fail++;
                    last;
                }
            }
        }
        if ($bytes) {
            $ssh2->blocking($old_blocking);
            return (wantarray ? @out : $out[0])
        }
        my $eof = $self->eof;
        if ($fail or $eof) {
            $ssh2->_set_error if $eof;
            $ssh2->blocking($old_blocking);
            return;
        }
        unless ($zero) {
            return unless $old_blocking;
            if ($deadline and time > $deadline) {
                $ssh2->_set_error(Net::SSH2::LIBSSH2_ERROR_TIMEOUT(), "Time out waiting for data");
                return;
            }
            return if $deadline and time > $deadline;
            my $sock = $ssh2->sock;
            my $fn = fileno($sock);
            my ($rbm, $wbm) = ('', '');
            my $bd = $ssh2->block_directions;
            vec($rbm, $fn, 1) = 1 if $bd & Net::SSH2::LIBSSH2_SESSION_BLOCK_INBOUND();
            vec($wbm, $fn, 1) = 1 if $bd & Net::SSH2::LIBSSH2_SESSION_BLOCK_OUTBOUND();
            select $rbm, $wbm, undef, $delay;
        }
    }
}

my $readline_non_blocking_warned;
sub readline {
    my ($self, $ext, $eol) = @_;
    return if $self->eof;
    $ext ||= 0;
    $eol = $/ unless @_ >= 3;

    $self->blocking or $readline_non_blocking_warned++ or
        warnings::warnif('Net::SSH2',
                         "Calling Net::SSH2::Channel::readline in non-blocking mode is usually a programming error");

    if (wantarray or not defined $eol) {
        my $data = '';
        my $buffer;
        while (1) {
            my $bytes = $self->read($buffer, 32768, $ext);
            last unless defined $bytes;
            if (!$bytes and $self->eof) {
                $self->session->_set_error(Net::SSH2::LIBSSH2_ERROR_NONE());
                last;
            }
            $data .= $buffer;
        }
        defined $eol and return split /(?<=\Q$eol\E)/s, $data;
        wantarray and not length $data and return ();
        return $data;
    }
    else {
        my $c;
        my $data = '';
        while (1) {
            $c = $self->getc($ext);
            last unless defined $c;
            $data .= $c;
            if ( (!length($c) and $self->eof) or
                 $data =~ /\Q$eol\E\z/) {
                $self->session->_set_error(Net::SSH2::LIBSSH2_ERROR_NONE());
                last;
            }
        }
        return (length $data ? $data : undef);
    }
}

sub wait_closed {
    my $self = shift;
    if ($self->wait_eof) {
        $self->flush('all');
        return $self->_wait_closed;
    }
    undef;
}

sub exit_status {
    my $self = shift;
    return unless $self->wait_closed;
    return $self->_exit_status;
}

sub exit_signal {
    my $self = shift;
    return unless $self->wait_closed;
    return $self->_exit_signal;
}

my %signal_number;
sub exit_signal_number {
    my $self = shift;
    my $signal = $self->exit_signal;
    return unless defined $signal;
    return 0 unless $signal;
    unless (%signal_number) {
        require Config;
        my @names = split /\s+/, $Config::Config{sig_name};
        @signal_number{@names} = 0..$#names;
    }
    $signal =~ s/\@\.[^\.]+\.config\.guess$//;
    my $number = $signal_number{$signal};
    $number = 255 unless defined $number;
    return $number;
}

my %pty_modes = (TTY_OP_END => 0, VINTR => 1, VQUIT => 2, VERASE => 3, VKILL => 4, VEOF => 5,
                 VEOL => 6, VEOL2 => 7, VSTART => 8, VSTOP => 9, VSUSP => 10, VDSUSP => 11,
                 VREPRINT => 12, VWERASE => 13, VLNEXT => 14, VFLUSH => 15, VSWTCH => 16, VSTATUS => 17,
                 VDISCARD => 18, IGNPAR => 30, PARMRK => 31, INPCK => 32, ISTRIP => 33, INLCR => 34,
                 IGNCR => 35, ICRNL => 36, IUCLC => 37, IXON => 38, IXANY => 39, IXOFF => 40,
                 IMAXBEL => 41, ISIG => 50, ICANON => 51, XCASE => 52, ECHO => 53, ECHOE => 54,
                 ECHOK => 55, ECHONL => 56, NOFLSH => 57, TOSTOP => 58, IEXTEN => 59, ECHOCTL => 60,
                 ECHOKE => 61, PENDIN => 62, OPOST => 70, OLCUC => 71, ONLCR => 72, OCRNL => 73,
                 ONOCR => 74, ONLRET => 75, CS7 => 90, CS8 => 91, PARENB => 92, PARODD => 93,
                 TTY_OP_ISPEED => 128, TTY_OP_OSPEED => 129);

sub pty {
    my $self = shift;
    if (defined $_[1] and ref $_[1] eq 'HASH') {
        my $term = shift;
        my $modes = shift;
        my $packed = '';
        while (my ($k, $v) = each %$modes) {
            unless ($k =~ /^\d+$/) {
                my $k1 = $pty_modes{uc $k};
                defined $k1 or croak "Invalid pty mode key '$k'";
                $k = $k1;
            }
            next if $k == 0; # ignore the TTY_OP_END marker
            $k > 159 and croak "Invalid pty mode key '$k'";
            $packed .= pack CN => $k, $v;
        }
        $self->_pty($term, "$packed\x00", @_);
    }
    else {
        $self->_pty(@_);
    }
}

# tie interface

sub PRINT {
    my $self = shift;
    my $sep = defined($,) ? $, : '';
    $self->write(join $sep, @_)
}

sub PRINTF {
    my $self = shift;
    my $template = shift;
    $self->write(sprintf $template, @_);
}

sub WRITE {
    my ($self, $buf, $len, $offset) = @_;
    $self->write(substr($buf, $offset || 0, $len))
}

sub READLINE { shift->readline(0, $/) }

sub READ {
    my ($self, undef, $len, $offset) = @_;
    my $bytes = $self->read(my($buffer), $len);
    substr($_[1], $offset || 0) = $buffer
        if defined $bytes;
    return $bytes;
}

sub BINMODE { 1 }

sub CLOSE {
    my $self = shift;
    my $ob = $self->blocking;
    $self->blocking(1);
    my $rc = undef;
    if ($self->close and
        $self->wait_closed) {
        my $status = $self->exit_status;
        my $signal = $self->exit_signal_number;
        $self->session->_set_error;
        $? = ($status << 8) | $signal;
        $rc = 1 if $? == 0;
    }
    $self->blocking($ob);
    $rc;
}

sub EOF {
    my $self = shift;
    $self->eof;
}

*GETC = \&getc;

1;
__END__

=head1 NAME

Net::SSH2::Channel - SSH2 channel object

=head1 SYNOPSIS

  my $chan = $ssh2->channel()
    or $ssh2->die_with_error;

  $chan->exec("ls -ld /usr/local/libssh2*")
    or $ssh2->die_with_error;

  $chan->send_eof;

  while (<$chan>) {
    print "line read: $_";
  }

  print "exit status: " . $chan->exit_status . "\n";

=head1 DESCRIPTION

A channel object is created by the L<Net::SSH2> C<channel> method.  As well
as being an object, it is also a tied filehandle.

=head2 setenv ( key, value ... )

Sets remote environment variables. Note that most servers do not allow
environment variables to be freely set.

Pass in a list of keys and values with the values to set.

It returns a true value if all the given environment variables were
correctly set.

=head2 blocking ( flag )

Enable or disable blocking.

Note that this is currently implemented in libssh2 by setting a
per-session flag. It's equivalent to L<Net::SSH2::blocking>.

=head2 eof

Returns true if the remote server sent an EOF.

=head2 send_eof

Sends an EOF to the remote side.

After an EOF has been sent, no more data may be
sent to the remote process C<STDIN> channel.

Note that if a PTY was requested for the channel, the EOF may be
ignored by the remote server. See L</pty>.

=head2 close

Close the channel (happens automatically on object destruction).

=head2 wait_closed

Wait for a remote close event.

In order to avoid a bug in libssh2 this method discards any unread
data queued in the channel.

=head2 exit_status

Returns the channel's program exit status.

This method blocks until the remote side closes the channel.

=head2 pty ( terminal [, modes [, width [, height ]]] )

Request a terminal on a channel.

C<terminal> is the type of emulation (e.g. vt102, ansi,
etc...).

C<modes> are the terminal mode modifiers, for instance:

    $c->pty('vt100', { echo => 0, vintr => ord('k') });

The list of acceptable mode modifiers is available from the SSH Connection
Protocol RFC (L<RFC4254|https://tools.ietf.org/html/rfc4254#section-8>).

If provided, C<width> and C<height> are the width and height in
characters (defaults to 80x24); if negative their absolute values
specify width and height in pixels.

=head2 pty_size ( width, height )

Request a terminal size change on a channel. C<width> and C<height> are the
width and height in characters; if negative their absolute values specify
width and height in pixels.

=head2 ext_data ( mode )

Set extended data handling mode:

=over 4

=item normal (default)

Keep data in separate channels; C<STDERR> is read separately.

=item ignore

Ignore all extended data.

=item merge

Merge into the regular channel.

=back

=head2 process ( request, message )

Start a process on the channel.  See also L<shell>, L<exec>, L<subsystem>.

Note that only one invocation of C<process> or any of the shortcuts
C<shell>, C<exec> or C<subsystem> is allowed per channel. In order to
run several commands, shells or/and subsystems, a new C<Channel>
instance must be used for every one.

Alternatively, it is also possible to launch a remote shell (using
L<shell>) and simulate the user interaction printing commands to its
C<stdin> stream and reading data back from its C<stdout> and
C<stderr>. But this approach should be avoided if possible; talking to
a shell is difficult and, in general, unreliable.

=head2 shell

Start a shell on the remote host (calls C<process("shell")>).

=head2 exec ( command )

Execute the command on the remote host (calls C<process("exec", command)>).

Note that the given command is parsed by the remote shell; it should
be properly quoted, specially when passing data from untrusted sources.

=head2 subsystem ( name )

Run subsystem on the remote host (calls C<process("subsystem", name)>).

=head2 read ( buffer, max_size [, ext ] )

Attempts to read up to C<max_size> bytes from the channel into C<buffer>. If
C<ext> is true, reads from the extended data channel (C<STDERR>).

The method returns as soon as some data is available, even if the
given size has not been reached.

Returns number of bytes read or C<undef> on failure. Note that 0 is a
valid return code.

=head2 read2 ( [max_size] )

Attempts to read from both the ordinary (stdout) and the extended
(stderr) channel streams.

Returns two scalars with the data read both from stdout and stderr. It
returns as soon as some data is available and any of the returned
values may be an empty string.

When some error happens it returns the empty list.

Example:

  my ($out, $err) = ('', '');
  while (!$channel->eof) {
      if (my ($o, $e) = $channel->read2) {
          $out .= $o;
          $err .= $e;
      }
      else {
          $ssh2->die_with_error;
      }
  }
  print "STDOUT:\n$out\nSTDERR:\n$err\n";

=head2 readline ( [ext [, eol ] ] )

Reads the next line from the selected stream (C<ext> defaults to 0:
stdout).

C<$/> is used as the end of line marker when C<eol> is C<undef>.

In list context reads and returns all the remaining lines until some
read error happens or the remote side sends an eof.

Note that this method is only safe when the complementary stream
(e.g. C<!ext>) is guaranteed to not generate data or when L</ext_data>
has been used to discard or merge it; otherwise it may hang. This is a
limitation of libssh2 that hopefully would be removed in a future
release, in the meantime you are advised to use L<read2> instead.

=head2 getc( [ext] )

Reads and returns the next character from the selected stream.

Returns C<undef> on error.

Note that due to some libssh2 quirks, the return value can be the
empty string which may indicate an EOF condition (but not
always!). See L</eof>.

=head2 write ( buffer )

Send the data in C<buffer> through the channel. Returns number of
bytes written, undef on failure.

In versions of this module prior to 0.57, when working in non-blocking
mode, the would-block condition was signaled by returning
C<LIBSSH2_ERROR_EAGAIN> (a negative number) while leaving the session
error status unset. From version 0.59, C<undef> is returned and the
session error status is set to C<LIBSSH2_ERROR_EAGAIN> as for any
other error.

In non-blocking mode, if C<write> fails with a C<LIBSSH2_ERROR_EAGAIN>
error, no other operation must be invoked over any object in the same
SSH session besides L</sock> and L<blocking_directions>.

Once the socket becomes ready again, the exact same former C<write>
call, with exactly the same arguments must be invoked.

Failing to do that would result in a corrupted SSH session. This is a
limitation in libssh2.

=head2 flush ( [ ext ] )

Discards the received but still unread data on the channel; if C<ext>
is present and set, discards data on the extended channel. Returns
number of bytes discarded, C<undef> on error.

=head2 exit_signal

Returns the name of exit signal from the remote command.

In list context returns also the error message and a language tag,
though as of libssh2 1.7.0, those values are always undef.

This method blocks until the remote side closes the channel.

=head2 exit_signal_number

Converts the signal name to a signal number using the local mapping
(which may be different to the remote one if the operating systems
differ).

=head2 window_read

Returns the number of bytes which the remote end may send without
overflowing the window limit.

In list context it also returns the number of bytes that are
immediately available for read and the size of the initial window.

=head2 window_write

Returns the number of bytes which may be safely written to the channel
without blocking at the SSH level. In list context it also returns the
size of the initial window.

Note that this method doesn't take into account the TCP connection
being used under the hood. Getting a positive integer back from this
method does not guarantee that such number of bytes could be written
to the channel without blocking the TCP connection.

=head2 receive_window_adjust (adjustment [, force])

Adjust the channel receive window by the given C<adjustment> bytes.

If the amount to be adjusted is less than C<LIBSSH2_CHANNEL_MINADJUST>
and force is false the adjustment amount will be queued for a later
packet.

On success returns the new size of the receive window. On failure it
returns C<undef>.

=head1 SEE ALSO

L<Net::SSH2>.

=cut
