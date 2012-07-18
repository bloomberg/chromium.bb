package MooseX::TransactionalMethods;
use Moose ();
use Moose::Exporter;
use aliased 'MooseX::Meta::Method::Transactional';
use Sub::Name;

our $VERSION = 0.008;

Moose::Exporter->setup_import_methods
  ( with_meta => [ 'transactional' ],
    also      => [ 'Moose' ],
  );


my $method_metaclass = Moose::Meta::Class->create_anon_class
  (
   superclasses => ['Moose::Meta::Method'],
   roles => [ Transactional ],
   cache => 1,
  );

sub transactional {
    my $meta = shift;
    my ($name, $schema, $code);

    if (ref($_[1]) eq 'CODE') {
        ($name, $code) = @_;
    } else {
        ($name, $schema, $code) = @_;
    }

    my $m = $method_metaclass->name->wrap
      (
       subname(join('::',$meta->name,$name),$code),
       package_name => $meta->name,
       name => $name,
       $schema ? (schema => $schema) : ()
      );

    $meta->add_method($name, $m);
}

1;


__END__

=head1 NAME

MooseX::TransactionalMethods - Syntax sugar for transactional methods

=head1 SYNOPSIS

  package Foo::Bar;
  use MooseX::TransactionalMethods; # includes Moose
  
  has schema => (is => 'ro');
  
  transactional foo => sub {
     # this is going to happen inside a transaction
  };

=head1 DESCRIPTION

This method exports the "transactional" declarator that will enclose
the method in a txn_do call.

=head1 DECLARATOR

=over

=item transactional $name => $code

When you declare with only the name and the coderef, the wrapper will
call 'schema' on your class to fetch the schema object on which it
will call txn_do to enclose your coderef.

=item transactional $name => $schema, $code

When you declare sending the schema object, it will store it in the
method metaclass and use it directly without any calls to this object.

NOTE THAT MIXING DECLARTIONS WITH SCHEMA AND WITHOUT SCHEMA WILL LEAD
TO PAINFULL CONFUSION SINCE THE WRAPPING IS SPECIFIC TO THAT CLASS AND
THE BEHAVIOR IS NOT MODIFIED WHEN YOU OVERRIDE THE METHOD. PREFER
USING THE DYNAMIC DECLARATOR WHEN POSSIBLE.

=back

=head1 AUTHORS

Daniel Ruoso E<lt>daniel@ruoso.comE<gt>

With help from rafl and doy from #moose.

=head1 COPYRIGHT AND LICENSE

Copyright 2010 by Daniel Ruoso et al

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
