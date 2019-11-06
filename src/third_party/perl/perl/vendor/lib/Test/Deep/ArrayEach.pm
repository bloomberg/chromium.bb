use strict;
use warnings;

package Test::Deep::ArrayEach;

use Test::Deep::Cmp;
use Scalar::Util ();

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

  return unless ref $got && Scalar::Util::reftype($got) eq 'ARRAY';
  my $exp = [ ($self->{val}) x @$got ];

  return Test::Deep::descend($got, $exp);
}

sub renderExp
{
  my $self = shift;
  my $exp = shift;

  return '[ ' . $self->SUPER::renderExp($self->{val}) . ', ... ]';
}

1;
