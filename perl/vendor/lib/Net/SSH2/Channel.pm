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


# tie interface

sub PRINT {
    my $self = shift;
    my $sep = defined($,) ? $, : '';
    $self->write(join $sep, @_)
}

sub PRINTF {
    my $self = shift;
    $self->write(sprintf @_)
}

sub WRITE {
    my ($self, $buf, $len, $offset) = @_;
    $self->write(substr($buf, $offset, $len))
}

sub READLINE {
    my $self = shift;
    return if $self->eof;

    if (wantarray) {
        my @lines;
        my $line;
        push @lines, $line while defined($line = $self->READLINE);
        return @lines;
    }
    
    my ($line, $eol, $c) = ('', $/);
    $line .= $c while $line !~ /\Q$eol\E$/ and defined($c = $self->GETC);
    length($line) ? $line : undef
}

sub GETC {
    my $self = shift;
    my $buf;
    my @poll = ({ handle => $self, events => 'in' });
    return
     unless $self->session->poll(250, \@poll) and $poll[0]->{revents}->{in};
    $self->read($buf, 1) ? $buf : undef
}

sub READ {
    my ($self, $rbuf, $len, $offset) = @_;
    my ($tmp, $count);
    return unless defined($count = $self->read($tmp, $len));
    substr($$rbuf, $offset) = $tmp;
    $count
}

sub CLOSE {
    &close
}

sub BINMODE {
}

sub EOF {
    &eof
}

1;
__END__

=head1 NAME

Net::SSH2::Channel - SSH 2 channel object

=head1 DESCRIPTION

A channel object is created by the L<Net::SSH2> C<channel> method.  As well
as being an object, it is also a tied filehandle.  The L<Net::SSH2> C<poll>
method can be used to check for read/write availability and other conditions.

=head2 setenv ( key, value ... )

Sets remote environment variables.  Note that most implementations do not allow
environment variables to be freely set.  Pass in a list of keys and values
with the values to set.  Returns the number of successful sets.

=head2 blocking ( flag )

Enable or disable blocking.  Note that this is currently implemented in libssh2
by setting a per-session flag; it's equivalent to L<Net::SSH2::blocking>.

=head2 eof

Returns true if the remote server sent an EOF.

=head2 send_eof

Send an EOF to the remote.  After an EOF has been sent, no more data may be
sent; the connection should be closed.

=head2 close

Close the channel (happens automatically on object destruction).

=head2 wait_closed

Wait for a remote close event.  Must have already seen remote EOF.

=head2 exit_status

Returns the channel's program exit status.

=head2 pty ( terminal [, modes [, width [, height ]]] )

Request a terminal on a channel.  If provided, C<width> and C<height> are the
width and height in characters (defaults to 80x24); if negative their absolute
values specify width and height in pixels.

=head2 pty_size ( width, height )

Request a terminal size change on a channel. C<width> and C<height> are the
width and height in characters; if negative their absolute values specify
width and height in pixels.

=head2 process ( request, message )

Start a process on the channel.  See also L<shell>, L<exec>, L<subsystem>.

=head2 shell

Start a shell on the remote host; calls L<process>("shell").

=head2 exec ( command )

Execute the command on the remote host; calls L<process>("exec", command).
Note that only one of these requests can succeed per channel (cp.
L<perlfunc/exec>); if you want to run a series of commands, consider using
L<shell> instead.

=head2 subsystem ( name )

Run subsystem on the remote host; calls L<process>("subsystem", command).

=head2 ext_data ( mode )

Set extended data handling mode:

=over 4

=item normal (default)

Keep data in separate channels; stderr is read separately.

=item ignore

Ignore all extended data.

=item merge

Merge into the regular channel.

=back

=head2 read ( buffer, size [, ext ] )

Attempts to read size bytes into the buffer.  Returns number of bytes read,
undef on failure.  If ext is present and set, reads from the extended data
channel (stderr).

=head2 write ( buffer [, ext ] )

Attempts to write the buffer to the channel.  Returns number of bytes written,
undef on failure.  If ext is present and set, writes to the extended data
channel (stderr).

=head2 flush ( [ ext ] )

Flushes the channel; if ext is present and set, flushes extended data channel.
Returns number of bytes flushed, undef on error.

=head2 exit_signal

Returns the exit signal of the command executed on the channel. Requires libssh
1.2.8 or higher.

=head1 SEE ALSO

L<Net::SSH2>.

=head1 AUTHOR

David B. Robins, E<lt>dbrobins@cpan.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2005, 2006 by David B. Robins; all rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.0 or,
at your option, any later version of Perl 5 you may have available.

=cut
