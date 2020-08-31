package # Hide from PAUSE
  DBIx::Class::SQLMaker::MySQL;

use warnings;
use strict;

use base qw( DBIx::Class::SQLMaker );

#
# MySQL does not understand the standard INSERT INTO $table DEFAULT VALUES
# Adjust SQL here instead
#
sub insert {
  my $self = shift;

  if (! $_[1] or (ref $_[1] eq 'HASH' and !keys %{$_[1]} ) ) {
    my $table = $self->_quote($_[0]);
    return "INSERT INTO ${table} () VALUES ()"
  }

  return $self->next::method (@_);
}

# Allow STRAIGHT_JOIN's
sub _generate_join_clause {
    my ($self, $join_type) = @_;

    if( $join_type && $join_type =~ /^STRAIGHT\z/i ) {
        return ' STRAIGHT_JOIN '
    }

    return $self->next::method($join_type);
}

my $force_double_subq;
$force_double_subq = sub {
  my ($self, $sql) = @_;

  require Text::Balanced;
  my $new_sql;
  while (1) {

    my ($prefix, $parenthesized);

    ($parenthesized, $sql, $prefix) = do {
      # idiotic design - writes to $@ but *DOES NOT* throw exceptions
      local $@;
      Text::Balanced::extract_bracketed( $sql, '()', qr/[^\(]*/ );
    };

    # this is how an error is indicated, in addition to crapping in $@
    last unless $parenthesized;

    if ($parenthesized =~ $self->{_modification_target_referenced_re}) {
      # is this a select subquery?
      if ( $parenthesized =~ /^ \( \s* SELECT \s+ /xi ) {
        $parenthesized = "( SELECT * FROM $parenthesized `_forced_double_subquery` )";
      }
      # then drill down until we find it (if at all)
      else {
        $parenthesized =~ s/^ \( (.+) \) $/$1/x;
        $parenthesized = join ' ', '(', $self->$force_double_subq( $parenthesized ), ')';
      }
    }

    $new_sql .= $prefix . $parenthesized;
  }

  return $new_sql . $sql;
};

sub update {
  my $self = shift;

  # short-circuit unless understood identifier
  return $self->next::method(@_) unless $self->{_modification_target_referenced_re};

  my ($sql, @bind) = $self->next::method(@_);

  $sql = $self->$force_double_subq($sql)
    if $sql =~ $self->{_modification_target_referenced_re};

  return ($sql, @bind);
}

sub delete {
  my $self = shift;

  # short-circuit unless understood identifier
  return $self->next::method(@_) unless $self->{_modification_target_referenced_re};

  my ($sql, @bind) = $self->next::method(@_);

  $sql = $self->$force_double_subq($sql)
    if $sql =~ $self->{_modification_target_referenced_re};

  return ($sql, @bind);
}

# LOCK IN SHARE MODE
my $for_syntax = {
   update => 'FOR UPDATE',
   shared => 'LOCK IN SHARE MODE'
};

sub _lock_select {
   my ($self, $type) = @_;

   my $sql = $for_syntax->{$type}
    || $self->throw_exception("Unknown SELECT .. FOR type '$type' requested");

   return " $sql";
}

1;
