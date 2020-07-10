use strict;
use warnings;

package Test::Deep::Isa;

use Test::Deep::Cmp;
use Scalar::Util;

sub init
{
  my $self = shift;

  my $val = shift;
  $self->{val} = $val;
}

sub descend
{
  my $self = shift;
  my $got = shift;

  return Scalar::Util::blessed($got)
    ? $got->isa($self->{val})
    : ref($got) eq $self->{val};
}

sub diag_message
{
  my $self = shift;

  my $where = shift;

  return "Checking class of $where with isa()";
}

sub renderExp
{
  my $self = shift;

  return "blessed into or ref of type '$self->{val}'";
}

1;
