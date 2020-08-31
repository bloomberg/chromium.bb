package Email::MIME::Kit::Role::Component;
# ABSTRACT: things that are kit components
$Email::MIME::Kit::Role::Component::VERSION = '3.000006';
use Moose::Role;

#pod =head1 DESCRIPTION
#pod
#pod All (or most, anyway) components of an Email::MIME::Kit will perform this role.
#pod Its primary function is to provide a C<kit> attribute that refers back to the
#pod Email::MIME::Kit into which the component was installed.
#pod
#pod =cut

has kit => (
  is  => 'ro',
  isa => 'Email::MIME::Kit',
  required => 1,
  weak_ref => 1,
);

no Moose::Role;
1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::MIME::Kit::Role::Component - things that are kit components

=head1 VERSION

version 3.000006

=head1 DESCRIPTION

All (or most, anyway) components of an Email::MIME::Kit will perform this role.
Its primary function is to provide a C<kit> attribute that refers back to the
Email::MIME::Kit into which the component was installed.

=head1 AUTHOR

Ricardo Signes <rjbs@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2018 by Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
