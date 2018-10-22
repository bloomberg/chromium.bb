package Net::SSH2::File;

use strict;
use warnings;
use Carp;

# methods

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
}

sub BINMODE {
}

sub EOF {
    0
}

1;
__END__

=head1 NAME

Net::SSH2::File - SSH 2 SFTP file object

=head1 DESCRIPTION

An SFTP file object is created by the L<Net::SSH2::SFTP> C<open> method.

=head2 read ( buffer, size )

Read size bytes from the file into a given buffer.  Returns number of bytes
read, or undef on failure.

=head2 write ( buffer )

Write buffer to the remote file; returns bytes written, undef on failure.

=head2 stat

Returns file attributes; see Net::SSH2::SFTP::stat.

=head2 setstat ( key, value... )

Sets file attributes; see Net::SSH2::SFTP::setstat.

=head2 seek ( offset )

Set the file pointer offset.

=head2 tell

Returns the current file pointer offset.

=head1 SEE ALSO

L<Net::SSH2::SFTP>.

=head1 AUTHOR

David B. Robins, E<lt>dbrobins@cpan.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2005, 2006 by David B. Robins; all rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.0 or,
at your option, any later version of Perl 5 you may have available.

=cut
