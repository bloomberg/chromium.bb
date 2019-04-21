package Module::Plan::Archive;

# Simple module for creating Module::Build::Plan archives, which are
# single-file packaged sets of tarballs with a build in p5i script.

use 5.006;
use strict;
use Params::Util       ();
use Archive::Tar       ();
use Module::Plan::Base ();

use vars qw{$VERSION};
BEGIN {
	$VERSION = '1.19';
}





#####################################################################
# Constructor and Accessors

sub new {
	my $class = shift;
	my $self  = bless { @_ }, $class;

	# Check params
	unless ( Params::Util::_INSTANCE($self->plan, 'Module::Plan::Base') ) {
		Carp("Did not provide a Module::Plan::Base object to Archive contructor");
	}
	unless ( $self->plan->can('fetch') ) {
		Carp("The plan does not implement a 'fetch' method");
	}

	return $self;
}

sub from_p5i {
	my $class = shift;

	# Create an archive from a file name
	my $file = shift;
	unless ( Params::Util::_STRING($file) and -f $file ) {
		Carp('Missing or invalid file name');
	}

	# Create the plan, and from that the archive
	return $class->new(
		plan => Module::Plan::Base->read( $file, @_ ),
	);
}

sub plan {
	$_[0]->{plan};
}

sub no_inject {
	$_[0]->plan->no_inject;
}





#####################################################################
# Archive Generation

sub save {
	my $self    = shift;
	my $file    = shift;
	my $archive = $self->archive;
	my $rv = $archive->write( $file, 9 );
	return 1;
}

sub archive {
	my $self = shift;
	my $plan = $self->plan;

	# Create the tarball and add the plan
	my $tar = Archive::Tar->new;
	$tar->add_data( 'default.p5i', $self->default_p5i );

	# Add the files
	foreach my $name ( $plan->names ) {
		unless ( $plan->dists_hash->{$name} ) {
			$plan->_fetch_uri($name);
		}

		# Read the dist into memory and add to tarball
		my $file   = $plan->dists_hash->{$name};
		my $buffer = '';
		SCOPE: {
			local $/ = undef;
			open( DIST, $file )       or die "open: $!";
			defined($buffer = <DIST>) or die "read: $!";
			close( DIST )             or die "close: $!";
		}
		$tar->add_data( $name, $buffer );
	}

	return $tar;
}

# Generate the new default.p5i plan file for the archive
sub default_p5i {
	my $self  = shift;
	my $class = $self->can('ref') ? $self->ref : ref($self);
	return join '', map { "$_\n" } ( $class, "", $self->plan->names );
}

1;
