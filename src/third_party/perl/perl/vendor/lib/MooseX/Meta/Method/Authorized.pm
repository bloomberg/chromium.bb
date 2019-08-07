package MooseX::Meta::Method::Authorized;
use MooseX::Meta::Method::Authorized::Meta::Role;
use Moose::Util::TypeConstraints;
use aliased 'MooseX::Meta::Method::Authorized::CheckRoles';

has requires =>
  ( is => 'ro',
    isa => 'ArrayRef',
    default => sub { [] } );

my $default_verifier = CheckRoles->new();
has verifier =>
  ( is => 'ro',
    isa => duck_type(['authorized_do']),
    default => sub { $default_verifier } );

around wrap => sub {
    my ($wrap, $method, $code, %options) = @_;

    my $meth_obj;
    $meth_obj = $method->$wrap
      (
       sub {
           $meth_obj->verifier->authorized_do($meth_obj, $code, @_)
       },
       %options
      );
    return $meth_obj;
};



1;

__END__

=head1 NAME

MooseX::Meta::Method::Authorized - Authorization in method calls

=head1 DESCRIPTION

This trait provides support for verifying authorization before calling
a method.

=head1 ATTRIBUTES

=over

=item requires

This attribute is an array reference with the values that are going to
be used by the verifier when checking this invocation.

=item verifier

This is the object/class on which the "authorized_do" method is going
to be invoked. This is the object responsible for doing the actual
verification. It is invoked as:

  $verifier->authorized_do($meth_obj, $code, @_)

It is expected that this method should die if the authorization is not
stablished.

The default value for this attribute is
L<MooseX::Meta::Method::Authorized::CheckRoles>, which will get the
current user by calling the "user" method and list the roles given to
that user by invoking the "roles" method.

=back

=head1 METHOD

=over

=item wrap

This role overrides wrap so that the actual method is only invoked
after the authorization being checked.

=back

=head1 SEE ALSO

L<MooseX::AuthorizedMethods>, L<Class::MOP::Method>

=head1 AUTHORS

Daniel Ruoso E<lt>daniel@ruoso.comE<gt>

With help from rafl and doy from #moose.

=head1 COPYRIGHT AND LICENSE

Copyright 2010 by Daniel Ruoso et al

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
