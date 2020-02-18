package # hide from PAUSE
    DBIx::Class::CDBICompat::Constraints;

use strict;
use warnings;

sub constrain_column {
  my $class = shift;
  my $col   = $class->find_column(+shift)
    or return $class->throw_exception("constraint_column needs a valid column");
  my $how = shift
    or return $class->throw_exception("constrain_column needs a constraint");
  if (ref $how eq "ARRAY") {
    my %hash = map { $_ => 1 } @$how;
    $class->add_constraint(list => $col => sub { exists $hash{ +shift } });
  } elsif (ref $how eq "Regexp") {
    $class->add_constraint(regexp => $col => sub { shift =~ $how });
  } else {
    $how =~ m/([^:]+)$/; # match is safe - we throw above on empty $how
    my $try_method = sprintf '_constrain_by_%s', lc $1; # $how->moniker;
    if (my $dispatch = $class->can($try_method)) {
      $class->$dispatch($col => ($how, @_));
    } else {
      $class->throw_exception("Don't know how to constrain $col with $how");
    }
  }
}

sub add_constraint {
  my $class = shift;
  $class->_invalid_object_method('add_constraint()') if ref $class;
  my $name = shift or return $class->throw_exception("Constraint needs a name");
  my $column = $class->find_column(+shift)
    or return $class->throw_exception("Constraint $name needs a valid column");
  my $code = shift
    or return $class->throw_exception("Constraint $name needs a code reference");
  return $class->throw_exception("Constraint $name '$code' is not a code reference")
    unless ref($code) eq "CODE";

  #$column->is_constrained(1);
  $class->add_trigger(
    "before_set_$column" => sub {
      my ($self, $value, $column_values) = @_;
      $code->($value, $self, $column, $column_values)
        or return $self->throw_exception(
        "$class $column fails '$name' constraint with '$value'");
    }
  );
}

1;
