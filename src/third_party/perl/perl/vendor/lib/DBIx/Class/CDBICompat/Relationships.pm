package # hide from PAUSE
    DBIx::Class::CDBICompat::Relationships;

use strict;
use warnings;
use base 'Class::Data::Inheritable';

use Clone;
use DBIx::Class::CDBICompat::Relationship;
use DBIx::Class::_Util qw(quote_sub perlstring);

__PACKAGE__->mk_classdata('__meta_info' => {});


=head1 NAME

DBIx::Class::CDBICompat::Relationships - Emulate has_a(), has_many(), might_have() and meta_info()

=head1 DESCRIPTION

Emulate C<has_a>, C<has_many>, C<might_have> and C<meta_info>.

=cut

sub has_a {
    my($self, $col, @rest) = @_;

    $self->_declare_has_a($col, @rest);
    $self->_mk_inflated_column_accessor($col);

    return 1;
}


sub _declare_has_a {
  my ($self, $col, $f_class, %args) = @_;
  $self->throw_exception( "No such column ${col}" )
   unless $self->has_column($col);
  $self->ensure_class_loaded($f_class);

  my $rel_info;

  # Class::DBI allows Non database has_a with implicit deflate and inflate
  # Hopefully the following will catch Non-database tables.
  if( !$f_class->isa('DBIx::Class::Row') and !$f_class->isa('Class::DBI::Row') ) {
    $args{'inflate'} ||= sub { $f_class->new(shift) }; # implicit inflate by calling new
    $args{'deflate'} ||= sub { shift() . '' }; # implicit deflate by stringification
  }

  if ($args{'inflate'} || $args{'deflate'}) { # Non-database has_a
    if (!ref $args{'inflate'}) {
      my $meth = $args{'inflate'};
      $args{'inflate'} = sub { $f_class->$meth(shift); };
    }
    if (!ref $args{'deflate'}) {
      my $meth = $args{'deflate'};
      $args{'deflate'} = sub { shift->$meth; };
    }
    $self->inflate_column($col, \%args);

    $rel_info = {
        class => $f_class
    };
  }
  else {
    $self->belongs_to($col, $f_class);
    $rel_info = $self->result_source_instance->relationship_info($col);
  }

  $rel_info->{args} = \%args;

  $self->_extend_meta(
    has_a => $col,
    $rel_info
  );

  return 1;
}

sub _mk_inflated_column_accessor {
    my($class, $col) = @_;

    return $class->mk_group_accessors('inflated_column' => $col);
}

sub has_many {
  my ($class, $rel, $f_class, $f_key, $args) = @_;

  my @f_method;

  if (ref $f_class eq 'ARRAY') {
    ($f_class, @f_method) = @$f_class;
  }

  if (ref $f_key eq 'HASH' && !$args) { $args = $f_key; undef $f_key; };

  $args ||= {};
  my $cascade = delete $args->{cascade} || '';
  if (delete $args->{no_cascade_delete} || $cascade eq 'None') {
    $args->{cascade_delete} = 0;
  }
  elsif( $cascade eq 'Delete' ) {
    $args->{cascade_delete} = 1;
  }
  elsif( length $cascade ) {
    warn "Unemulated cascade option '$cascade' in $class->has_many($rel => $f_class)";
  }

  if( !$f_key and !@f_method ) {
      $class->ensure_class_loaded($f_class);
      my $f_source = $f_class->result_source_instance;
      ($f_key) = grep { $f_source->relationship_info($_)->{class} eq $class }
                      $f_source->relationships;
  }

  $class->next::method($rel, $f_class, $f_key, $args);

  my $rel_info = $class->result_source_instance->relationship_info($rel);
  $args->{mapping}      = \@f_method;
  $args->{foreign_key}  = $f_key;
  $rel_info->{args} = $args;

  $class->_extend_meta(
    has_many => $rel,
    $rel_info
  );

  if (@f_method) {
    quote_sub "${class}::${rel}", sprintf( <<'EOC', perlstring $rel), { '$rf' => \sub { my $o = shift; $o = $o->$_ for @f_method; $o } };
      my $rs = shift->search_related( %s => @_);
      $rs->{attrs}{record_filter} = $rf;
      return (wantarray ? $rs->all : $rs);
EOC

    return 1;
  }
}


sub might_have {
  my ($class, $rel, $f_class, @columns) = @_;

  my $ret;
  if (ref $columns[0] || !defined $columns[0]) {
    $ret = $class->next::method($rel, $f_class, @columns);
  } else {
    $ret = $class->next::method($rel, $f_class, undef,
                                { proxy => \@columns });
  }

  my $rel_info = $class->result_source_instance->relationship_info($rel);
  $rel_info->{args}{import} = \@columns;

  $class->_extend_meta(
    might_have => $rel,
    $rel_info
  );

  return $ret;
}


sub _extend_meta {
    my ($class, $type, $rel, $val) = @_;
    my %hash = %{ Clone::clone($class->__meta_info || {}) };

    $val->{self_class} = $class;
    $val->{type}       = $type;
    $val->{accessor}   = $rel;

    $hash{$type}{$rel} = DBIx::Class::CDBICompat::Relationship->new($val);
    $class->__meta_info(\%hash);
}


sub meta_info {
    my ($class, $type, $rel) = @_;
    my $meta = $class->__meta_info;
    return $meta unless $type;

    my $type_meta = $meta->{$type};
    return $type_meta unless $rel;
    return $type_meta->{$rel};
}


sub search {
  my $self = shift;
  my $attrs = {};
  if (@_ > 1 && ref $_[$#_] eq 'HASH') {
    $attrs = { %{ pop(@_) } };
  }
  my $where = (@_ ? ((@_ == 1) ? ((ref $_[0] eq "HASH") ? { %{+shift} } : shift)
                               : {@_})
                  : undef());
  if (ref $where eq 'HASH') {
    foreach my $key (keys %$where) { # has_a deflation hack
      $where->{$key} = ''.$where->{$key}
        if eval { $where->{$key}->isa('DBIx::Class') };
    }
  }
  $self->next::method($where, $attrs);
}

sub new_related {
  return shift->search_related(shift)->new_result(shift);
}

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.

=cut

1;
