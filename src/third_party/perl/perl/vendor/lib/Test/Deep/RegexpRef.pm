use strict;
use warnings;

package Test::Deep::RegexpRef;

use Test::Deep::Ref;
use Test::Deep::RegexpVersion;

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

	my $exp = $self->{val};

	if ($Test::Deep::RegexpVersion::OldStyle) {
		return 0 unless $self->test_class($got, "Regexp");
		return 0 unless $self->test_reftype($got, "SCALAR");
	} else {
		return 0 unless $self->test_reftype($got, "REGEXP");
	}

	return Test::Deep::descend($got, Test::Deep::regexprefonly($exp));
}

sub renderGot
{
	my $self = shift;

	return shift()."";
}

1;
