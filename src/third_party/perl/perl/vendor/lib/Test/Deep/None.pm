use strict;
use warnings;

package Test::Deep::None;

use Test::Deep::Cmp;

sub init
{
  my $self = shift;

  my @list = map {
    eval { $_->isa('Test::Deep::None') }
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
    return 0 if Test::Deep::eq_deeply_cache($got, $cmp);
  }

  return 1;
}

sub renderExp
{
  my $self = shift;

  my @expect = map {; Test::Deep::wrap($_) } @{ $self->{val} };
  my $things = join(", ", map {$_->renderExp} @expect);

  return "None of ( $things )";
}

sub diagnostics
{
  my $self = shift;
  my ($where, $last) = @_;

  my $got = $self->renderGot($last->{got});
  my $exp = $self->renderExp;

  my $diag = <<EOM;
Comparing $where with None
got      : $got
expected : $exp
EOM

  $diag =~ s/\n+$/\n/;
  return $diag;
}

1;
