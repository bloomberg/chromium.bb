package # Hide from PAUSE
  DBIx::Class::SQLMaker::ACCESS;

use strict;
use warnings;
use base 'DBIx::Class::SQLMaker';

# inner joins must be prefixed with 'INNER '
sub new {
  my $class = shift;
  my $self  = $class->next::method(@_);

  $self->{_default_jointype} = 'INNER';

  return $self;
}

# MSAccess is retarded wrt multiple joins in FROM - it requires a certain
# way of parenthesizing each left part before each next right part
sub _recurse_from {
  my @j = shift->_gen_from_blocks(@_);

  # first 2 steps need no parenthesis
  my $fin_join = join (' ', splice @j, 0, 2);

  while (@j) {
    $fin_join = sprintf '( %s ) %s', $fin_join, (shift @j);
  }

  # the entire FROM is *ALSO* expected parenthesized
  "( $fin_join )";
}

1;
