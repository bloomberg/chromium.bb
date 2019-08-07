package Test::Object::Test;

use strict;
use Carp         ();
use Scalar::Util ();

use vars qw{$VERSION};
BEGIN {
	$VERSION = '0.07';
}





#####################################################################
# Constructor and Accessors

sub new {
	my $class = shift;
	my $self  = bless { @_ }, $class;

	# Check params
	unless ( _CLASS($self->class) ) {
		Carp::croak("Did not provide a valid test class");
	}
	unless ( _CODELIKE($self->code) ) {
		Carp::croak("Did not provide a valid CODE or callable object");
	}

	$self;
}

sub class {
	$_[0]->{class};
}

sub tests {
	$_[0]->{tests};
}

sub code {
	$_[0]->{code};
}





#####################################################################
# Main Methods

sub run {
	$_[0]->code->( $_[1] );
}





#####################################################################
# Support Functions

# Stolen from Params::Util to avoid adding a dependency needlessly

sub _CLASS ($) {
	(defined $_[0] and ! ref $_[0] and $_[0] =~ m/^[^\W\d]\w*(?:::\w+)*$/s) ? $_[0] : undef;
}

sub _CODELIKE {
	(Scalar::Util::reftype($_[0])||'') eq 'CODE'
	or
	Scalar::Util::blessed($_[0]) and overload::Method($_[0],'&{}')
	? $_[0] : undef;
}

1;
