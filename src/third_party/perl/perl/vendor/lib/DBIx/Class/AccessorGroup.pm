package DBIx::Class::AccessorGroup;

use strict;
use warnings;

use base qw/Class::Accessor::Grouped/;
use Scalar::Util qw/weaken blessed/;
use namespace::clean;

my $successfully_loaded_components;

sub get_component_class {
  my $class = $_[0]->get_inherited($_[1]);

  # It's already an object, just go for it.
  return $class if blessed $class;

  if (defined $class and ! $successfully_loaded_components->{$class} ) {
    $_[0]->ensure_class_loaded($class);

    no strict 'refs';
    $successfully_loaded_components->{$class}
      = ${"${class}::__LOADED__BY__DBIC__CAG__COMPONENT_CLASS__"}
        = do { \(my $anon = 'loaded') };
    weaken($successfully_loaded_components->{$class});
  }

  $class;
};

sub set_component_class {
  shift->set_inherited(@_);
}

1;

=head1 NAME

DBIx::Class::AccessorGroup - See Class::Accessor::Grouped

=head1 SYNOPSIS

=head1 DESCRIPTION

This class now exists in its own right on CPAN as Class::Accessor::Grouped

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.

=cut
