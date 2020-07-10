package Net::IMAP::Client::MsgAddress;

use Encode ();

sub new {
    my ($class, $struct) = @_;
    my $self = {
        name           => $struct->[0],
        at_domain_list => $struct->[1],
        mailbox        => $struct->[2],
        host           => $struct->[3],
    };
    bless $self, $class;
}

sub _decode {
    my ($str) = @_;
    if (defined($str)) {
        eval { $str = Encode::decode('MIME-Header', $str); };
    }
    return $str;
}

use overload q("") => \&as_string;

sub name { _decode($_[0]->{name}) }
sub at_domain_list { _decode($_[0]->{at_domain_list}) }
sub mailbox { _decode($_[0]->{mailbox}) }
sub host { _decode($_[0]->{host}) }

sub email {
    my ($self) = @_;
    return $self->mailbox . '@' . $self->host;
}

sub as_string {
    my ($self) = @_;
    my $str;
    if (($str = $self->name)) {
        $str .= ' <' . $self->email . '>';
    } else {
        $str = $self->email;
    }
    return $str;
}

1;

__END__

=pod

=head1 NAME

Net::IMAP::Client::MsgAddress

=head1 DESCRIPTION

Represents one email address as returned in an ENVELOPE part of the
FETCH command.

When used in string context, this object magically translates to
S<"Full Name E<lt>email@address.comE<gt>">.

=head1 METHODS

=head2 C<new>  # constructor

Creates a new object from the given array.

=over

=item name

Returns the full name, if any

=item at_domain_list

Returns the "at_domain_list" part (WTF is that?)

=item mailbox

Returns the mailbox name (i.e. the 'email' part in the example above).

=item host

Returns the host name (i.e. the 'address.com' part).

=item email

Returns the full email address: C<$self->{mailbox} . '@' . $self->{host}>.

=item as_string

Used by stringification.  It returns the full name and email address
as in S<"Full Name E<lt>email@address.comE<gt>">.

=back

=head1 SEE ALSO

L<Net::IMAP::Client>, L<Net::IMAP::Client::MsgSummary>

=head1 AUTHOR

Mihai Bazon, <mihai.bazon@gmail.com>
    http://www.dynarchlib.com/
    http://www.bazon.net/mishoo/

=head1 COPYRIGHT

Copyright (c) Mihai Bazon 2008.  All rights reserved.

This module is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=head1 DISCLAIMER OF WARRANTY

BECAUSE THIS SOFTWARE IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY
FOR THE SOFTWARE, TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT
WHEN OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER
PARTIES PROVIDE THE SOFTWARE "AS IS" WITHOUT WARRANTY OF ANY KIND,
EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE
SOFTWARE IS WITH YOU. SHOULD THE SOFTWARE PROVE DEFECTIVE, YOU ASSUME
THE COST OF ALL NECESSARY SERVICING, REPAIR, OR CORRECTION.

IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR
REDISTRIBUTE THE SOFTWARE AS PERMITTED BY THE ABOVE LICENCE, BE LIABLE
TO YOU FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL, OR
CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR INABILITY TO USE THE
SOFTWARE (INCLUDING BUT NOT LIMITED TO LOSS OF DATA OR DATA BEING
RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR THIRD PARTIES OR A
FAILURE OF THE SOFTWARE TO OPERATE WITH ANY OTHER SOFTWARE), EVEN IF
SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
DAMAGES.

=cut
