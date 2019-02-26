package Module::P5Z;

use 5.006;
use strict;
use File::pushd  ();
use Archive::Tar ();

use vars qw{$VERSION};
BEGIN {
	$VERSION = '1.19';
}





#####################################################################
# Constructor

sub read {
	my $class = shift;
	my $self  = bless { @_ }, $class;

	# Apply defaults
	$self->{tempd} ||= File::pushd::tempd();

	$self;
}

sub tempd {
	$_[0]->{tempd};
}

1;
