package # hide from PAUSE
    DBIx::Class::CDBICompat::ColumnCase;

use strict;
use warnings;

sub _register_column_group {
  my ($class, $group, @cols) = @_;
  return $class->next::method($group => map lc, @cols);
}

sub add_columns {
  my ($class, @cols) = @_;
  return $class->result_source_instance->add_columns(map lc, @cols);
}

sub has_a {
    my($self, $col, @rest) = @_;

    $self->_declare_has_a(lc $col, @rest);
    $self->_mk_inflated_column_accessor($col);

    return 1;
}

sub has_many {
  my ($class, $rel, $f_class, $f_key, @rest) = @_;
  return $class->next::method(
    $rel,
    $f_class,
    (ref($f_key) ?
      $f_key :
      lc($f_key||'')
    ),
    @rest
  );
}

sub get_inflated_column {
  my ($class, $get, @rest) = @_;
  return $class->next::method(lc($get), @rest);
}

sub store_inflated_column {
  my ($class, $set, @rest) = @_;
  return $class->next::method(lc($set), @rest);
}

sub set_inflated_column {
  my ($class, $set, @rest) = @_;
  return $class->next::method(lc($set), @rest);
}

sub get_column {
  my ($class, $get, @rest) = @_;
  return $class->next::method(lc($get), @rest);
}

sub set_column {
  my ($class, $set, @rest) = @_;
  return $class->next::method(lc($set), @rest);
}

sub store_column {
  my ($class, $set, @rest) = @_;
  return $class->next::method(lc($set), @rest);
}

sub find_column {
  my ($class, $col) = @_;
  return $class->next::method(lc($col));
}

# _build_query
#
# Build a query hash for find, et al. Overrides Retrieve::_build_query.

sub _build_query {
  my ($self, $query) = @_;

  my %new_query;
  $new_query{lc $_} = $query->{$_} for keys %$query;

  return \%new_query;
}

sub _deploy_accessor {
  my($class, $name, $accessor) = @_;

  return if $class->_has_custom_accessor($name);

         $class->next::method(lc $name   => $accessor);
  return $class->next::method($name      => $accessor);
}


sub new {
  my ($class, $attrs, @rest) = @_;
  my %att;
  $att{lc $_} = $attrs->{$_} for keys %$attrs;
  return $class->next::method(\%att, @rest);
}

1;
