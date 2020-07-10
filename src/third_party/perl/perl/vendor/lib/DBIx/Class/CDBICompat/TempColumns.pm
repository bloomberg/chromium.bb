package # hide from PAUSE
    DBIx::Class::CDBICompat::TempColumns;

use strict;
use warnings;
use base qw/Class::Data::Inheritable/;

use Carp;

__PACKAGE__->mk_classdata('_temp_columns' => { });

sub _add_column_group {
  my ($class, $group, @cols) = @_;

  return $class->next::method($group, @cols) unless $group eq 'TEMP';

  my %new_cols = map { $_ => 1 } @cols;
  my %tmp_cols = %{$class->_temp_columns};

  for my $existing_col ( grep $new_cols{$_}, $class->columns ) {
      # Already been declared TEMP
      next if $tmp_cols{$existing_col};

      carp "Declaring column $existing_col as TEMP but it already exists";
  }

  $class->_register_column_group($group => @cols);
  $class->mk_group_accessors('temp' => @cols);

  $class->_temp_columns({ %tmp_cols, %new_cols });
}

sub new {
  my ($class, $attrs, @rest) = @_;

  my $temp = $class->_extract_temp_data($attrs);

  my $new = $class->next::method($attrs, @rest);

  $new->set_temp($_, $temp->{$_}) for keys %$temp;

  return $new;
}

sub _extract_temp_data {
  my($self, $data) = @_;

  my %temp;
  foreach my $key (keys %$data) {
    $temp{$key} = delete $data->{$key} if $self->_temp_columns->{$key};
  }

  return \%temp;
}

sub find_column {
  my ($class, $col, @rest) = @_;
  return $col if $class->_temp_columns->{$col};
  return $class->next::method($col, @rest);
}

sub set {
  my($self, %data) = @_;

  my $temp_data = $self->_extract_temp_data(\%data);

  $self->set_temp($_, $temp_data->{$_}) for keys %$temp_data;

  return $self->next::method(%data);
}

sub get_temp {
  my ($self, $column) = @_;
  $self->throw_exception( "Can't fetch data as class method" ) unless ref $self;
  $self->throw_exception( "No such TEMP column '${column}'" ) unless $self->_temp_columns->{$column} ;
  return $self->{_temp_column_data}{$column}
    if exists $self->{_temp_column_data}{$column};
  return undef;
}

sub set_temp {
  my ($self, $column, $value) = @_;
  $self->throw_exception( "No such TEMP column '${column}'" )
    unless $self->_temp_columns->{$column};
  $self->throw_exception( "set_temp called for ${column} without value" )
    if @_ < 3;
  return $self->{_temp_column_data}{$column} = $value;
}

sub has_real_column {
  return 1 if shift->has_column(shift);
}

1;
