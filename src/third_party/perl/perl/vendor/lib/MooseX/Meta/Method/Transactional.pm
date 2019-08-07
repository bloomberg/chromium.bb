package MooseX::Meta::Method::Transactional;
use MooseX::Meta::Method::Transactional::Meta::Role;
use Moose::Util::TypeConstraints;

subtype 'SchemaGenerator',
  as 'CodeRef';
coerce 'SchemaGenerator',
  from duck_type(['txn_do']),
  via { my $schema = $_; sub { $schema } };

has schema =>
  ( is => 'ro',
    isa => 'SchemaGenerator',
    default => sub { sub { shift->schema } },
    coerce => 1 );

around 'wrap' => sub {
    my ($wrap, $method, $code, %options) = @_;

    my $meth_obj;
    $meth_obj = $method->$wrap
      (
       sub {
           my ($self) = @_;
           $meth_obj->schema->($self)->txn_do($code, @_);
       },
       %options
      );
    return $meth_obj;
};

1;

__END__

=head1 NAME

MooseX::Meta::Method::Transactional - Transactional methods trait

=head1 DESCRIPTION

This Role wraps methods in transactions to be used with DBIx::Class,
KiokuDB or any other object providing a txn_do method.

=head1 METHOD

=over

=item wrap

This role overrides wrap so that the actual method is wrapped in a
txn_do call. It uses the 'schema' accessor to obtain the object in
which it will call txn_do.

=back

=head1 ATTRIBUTES

=over

=item schema

This attribute contains a CodeRef that should return the schema
object. It can be used to pass a schema object when it can be defined
in compile-time, otherwise it will call "schema" on the object
instance to find it.

=back

=head1 SEE ALSO

L<MooseX::TransactionalMethods>, L<Class::MOP::Method>

=head1 AUTHORS

Daniel Ruoso E<lt>daniel@ruoso.comE<gt>

With help from rafl and doy from #moose.

=head1 COPYRIGHT AND LICENSE

Copyright 2010 by Daniel Ruoso et al

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
