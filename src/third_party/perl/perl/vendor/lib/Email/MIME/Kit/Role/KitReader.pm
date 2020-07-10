package Email::MIME::Kit::Role::KitReader;
# ABSTRACT: things that can read kit contents
$Email::MIME::Kit::Role::KitReader::VERSION = '3.000006';
use Moose::Role;
with 'Email::MIME::Kit::Role::Component';

#pod =head1 IMPLEMENTING
#pod
#pod This role also performs L<Email::MIME::Kit::Role::Component>.
#pod
#pod Classes implementing this role must provide a C<get_kit_entry> method.  It will
#pod be called with one parameter, the name of a path to an entry in the kit.  It
#pod should return a reference to a scalar holding the contents (as octets) of the
#pod named entry.  If no entry is found, it should raise an exception.
#pod
#pod A method called C<get_decoded_kit_entry> is provided.  It behaves like
#pod C<get_kit_entry>, but assumes that the entry for that name is stored in UTF-8
#pod and will decode it to text before returning.
#pod
#pod =cut

requires 'get_kit_entry';

sub get_decoded_kit_entry {
  my ($self, $name) = @_;
  my $content_ref = $self->get_kit_entry($name);

  require Encode;
  my $decoded = Encode::decode('utf-8', $$content_ref);
  return \$decoded;
}

no Moose::Role;
1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::MIME::Kit::Role::KitReader - things that can read kit contents

=head1 VERSION

version 3.000006

=head1 IMPLEMENTING

This role also performs L<Email::MIME::Kit::Role::Component>.

Classes implementing this role must provide a C<get_kit_entry> method.  It will
be called with one parameter, the name of a path to an entry in the kit.  It
should return a reference to a scalar holding the contents (as octets) of the
named entry.  If no entry is found, it should raise an exception.

A method called C<get_decoded_kit_entry> is provided.  It behaves like
C<get_kit_entry>, but assumes that the entry for that name is stored in UTF-8
and will decode it to text before returning.

=head1 AUTHOR

Ricardo Signes <rjbs@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2018 by Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
