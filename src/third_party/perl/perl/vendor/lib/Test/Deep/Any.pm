use strict;
use warnings;

package Test::Deep::Any;

use Scalar::Util ();
use Test::Deep::Cmp;

sub init
{
  my $self = shift;

  my @list = map {
    (Scalar::Util::blessed($_) && $_->isa('Test::Deep::Any'))
    ? @{ $_->{val} }
    : $_
  } @_;

  $self->{val} = \@list;
}

sub descend
{
  my $self = shift;
  my $got = shift;

  foreach my $cmp (@{$self->{val}})
  {
    return 1 if Test::Deep::eq_deeply_cache($got, $cmp);
  }

  return 0;
}

sub renderExp
{
  my $self = shift;

  my @expect = map {; Test::Deep::wrap($_) } @{ $self->{val} };
  my $things = join(", ", map {$_->renderExp} @expect);

  return "Any of ( $things )";
}

sub diagnostics
{
  my $self = shift;
  my ($where, $last) = @_;

  my $got = $self->renderGot($last->{got});
  my $exp = $self->renderExp;

  my $diag = <<EOM;
Comparing $where with Any
got      : $got
expected : $exp
EOM

  $diag =~ s/\n+$/\n/;
  return $diag;
}

4;
