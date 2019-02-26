package MooseX::Meta::Method::Authorized::CheckRoles;
use Moose;
use List::Util qw/first/;
use UNIVERSAL;

sub authorized_do {
    my $self = shift;
    my $method = shift;
    my $roles = $method->requires;
    my $code = shift;

    my ($instance) = @_;
    my $user = $instance->user;
    if (grep { my $r = $_;
               (grep { $r eq $_ } @$roles) ? 1 : 0 } $user->roles) {
        $code->(@_);
    } else {
        my $message = 'Access Denied. User';
        if ($user->can('id')) {
            $message .= ' "'.$user->id.'"';
        }
        $message .= ' does not have any of the required roles ('
          .(join ',', map { '"'.$_.'"' } @$roles)
            .') required to invoke method "'.$method->name
              .'" on class "'.$method->package_name.'". User roles are: ('
                .(join ',', map { '"'.$_.'"' } $user->roles).')';
        die $message;
    }

}

1;

__END__

=head1 NAME

MooseX::Meta::Method::Authorized::CheckRoles - Check roles of the user

=head1 DESCRIPTION

This verifier module will check if the user has any of the roles
defined in the "requires" attribute of the method. To get the user
this module will call "user" on the object which is the invocant for
this method, to get the roles it will call "roles" on the user object.

=head1 METHODS

=over

=item authorized_do($method, $code, @_)

This is the method that does the actual verification. It only invokes
the coderef after checking if the user has any of the required
roles. It will die otherwise with a string like:

  Access Denied. User "johndoe" does not have any of the required
  roles ("foo") required to invoke method "bla" on class
  "My::ClassTest1". User roles are: ("foo","bar","baz")

It will only show the user id if the user implements the method "id".

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
