package Email::Sender::Role::HasMessage;
# ABSTRACT: an object that has a message
$Email::Sender::Role::HasMessage::VERSION = '1.300031';
use Moo::Role;

#pod =attr message
#pod
#pod This attribute is a message associated with the object.
#pod
#pod =cut

has message => (
  is       => 'ro',
  required => 1,
);

no Moo::Role;
1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::Sender::Role::HasMessage - an object that has a message

=head1 VERSION

version 1.300031

=head1 ATTRIBUTES

=head2 message

This attribute is a message associated with the object.

=head1 AUTHOR

Ricardo Signes <rjbs@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2017 by Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
