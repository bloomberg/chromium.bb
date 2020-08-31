use strict;
use warnings;

package Test::Deep::All;

use Scalar::Util ();
use Test::Deep::Cmp;

sub init
{
  my $self = shift;

  my @list = map {
    (Scalar::Util::blessed($_) && $_->isa('Test::Deep::All'))
    ? @{ $_->{val} }
    : $_
  } @_;

  $self->{val} = \@list;
}

sub descend
{
  my $self = shift;
  my $got = shift;

  my $data = $self->data;

  my $index = 1;

  foreach my $cmp (@{$self->{val}})
  {
    $data->{index} = $index;
    $index++;

    next if Test::Deep::descend($got, $cmp);
    return 0
  }

  return 1;
}

sub render_stack
{
  my $self = shift;
  my $var = shift;
  my $data = shift;

  my $max = @{$self->{val}};

  return "(Part $data->{index} of $max in $var)";
}

1;
