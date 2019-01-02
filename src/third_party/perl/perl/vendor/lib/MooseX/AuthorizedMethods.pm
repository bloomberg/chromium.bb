package MooseX::AuthorizedMethods;
use Moose ();
use Moose::Exporter;
use aliased 'MooseX::Meta::Method::Authorized';
use Sub::Name;

our $VERSION = 0.006;

Moose::Exporter->setup_import_methods
  ( with_meta => [ 'authorized' ],
    also      => [ 'Moose' ],
  );


my $method_metaclass = Moose::Meta::Class->create_anon_class
  (
   superclasses => ['Moose::Meta::Method'],
   roles => [ Authorized ],
   cache => 1,
  );

sub authorized {
    my ($meta, $name, $requires, $code, %extra_options) = @_;

    my $m = $method_metaclass->name->wrap
      (
       subname(join('::',$meta->name,$name),$code),
       package_name => $meta->name,
       name => $name,
       requires => $requires,
       %extra_options,
      );

    $meta->add_method($name, $m);
}

1;


__END__

=head1 NAME

MooseX::AuthorizedMethods - Syntax sugar for authorized methods

=head1 SYNOPSIS

  package Foo::Bar;
  use MooseX::AuthorizedMethods; # includes Moose
  
  has user => (is => 'ro');
  
  authorized foo => ['foo'], sub {
     # this is going to happen only if the user has the 'foo' role
  };

=head1 DESCRIPTION

This method exports the "authorized" declarator that makes a
verification if the user has the required permissions before the acual
invocation. The default verification method will take the "user"
method result and call "roles" to list the roles given to that user.

=head1 DECLARATOR

=over

=item authorized $name => [qw(required permissions)], $code

This declarator will use the default verifier to check if the user has
one of the given roles, it will die otherwise.

=back

=head1 CUSTOM VERIFIERS

The default verifier used is
L<MooseX::Meta::Method::Authorized::CheckRoles>, you might send an
additional "verifier" option to the declarator with another object or
class. A verifier is simply a duck type with the "authorized_do"
method that is called as:

  $verifier->authorized_do($method, $code, @_)

It is expected that the verifier code die if the user doesn't fulfill
the authorization requests.

=head1 AUTHORS

Daniel Ruoso E<lt>daniel@ruoso.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2010 by Daniel Ruoso et al

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
