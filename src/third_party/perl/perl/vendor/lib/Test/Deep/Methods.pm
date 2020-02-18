use strict;
use warnings;

package Test::Deep::Methods;

use Test::Deep::Cmp;
use Scalar::Util;

sub init
{
  my $self = shift;

  # get them all into [$name,@args] => $value format
  my @methods;
  while (@_)
  {
    my $name = shift;
    my $value = shift;
    push(@methods,
      [
        ref($name) ? $name : [ $name ],
        $value
      ]
    );
  }
  $self->{methods} = \@methods;
}

sub descend
{
  my $self = shift;
  my $got = shift;

  my $data = $self->data;

  foreach my $method (@{$self->{methods}})
  {
    $data->{method} = $method;

    my ($call, $exp_res) = @$method;
    my ($name, @args) = @$call;

    local $@;

    my $got_res;
    if (! eval { $got_res = $self->call_method($got, $call); 1 }) {
      die $@ unless $@ =~ /\ACan't locate object method "\Q$name"/;
      $got_res = $Test::Deep::DNE;
    }

    next if Test::Deep::descend($got_res, $exp_res);

    return 0;
  }

  return 1;
}

sub call_method
{
  my $self = shift;
  my ($got, $call) = @_;
  my ($name, @args) = @$call;

  return $got->$name(@args);
}

sub render_stack
{
  my $self = shift;
  my ($var, $data) = @_;

  my $method = $data->{method};
  my ($call, $expect) = @$method;
  my ($name, @args) = @$call;

  my $args = @args ? "(".join(", ", @args).")" : "";
  $var .= "->$name$args";

  return $var;
}

1;
