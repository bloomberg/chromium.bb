package Module::Plan::Lite;

=pod

=head1 NAME

Module::Plan::Lite - Lite installation scripts for third-party modules

=head1 SYNOPSIS

The following is the contents of your default.pip file.

  Module::Plan::Lite
  
  # Everything in the plan file is installed in order
  
  # Supported file forms
  Install-This-First-1.00.tar.gz
  Install-This-Second.1.31.tar.gz
  extensions/This-This-0.02.tar.gz
  /absolute/Module-Location-4.12.tar.gz
  
  # Supported URI types
  ftp://foo.com/pip-0.13.tar.gz
  http://foo.com/pip-0.13.tar.gz
  
  # Support for PAR installation and conventions
  http://foo.com/DBI-1.37-MSWin32-5.8.0.par
  http://foo.com/DBI-1.37
  cpan://SMUELLER/PAR-Packer-0.975

=cut

use strict;
use URI                ();
use Module::Plan::Base ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.19';
	@ISA     = 'Module::Plan::Base';
}





#####################################################################
# Constructor

sub new {
	my $class = shift;
	my $self  = $class->SUPER::new(@_);

	# Parsing here isn't the best, but this is Lite after all
	foreach ( $self->lines ) {
		# Strip whitespace and comments
		next if /^\s*(?:\#|$)/;

		# Create the URI
		my $uri = URI->new_abs( $_, $self->p5i_uri );
		unless ( $uri ) {
			croak("Failed to get the URI for $_");
		}

		# Add the uri
		$self->add_uri( $uri );
	}

	$self;
}

sub fetch {
	my $self = shift;

	# Download the needed modules
	foreach my $name ( $self->names ) {
		next if $self->{dists}->{$name};
		$self->_fetch_uri($name);
	}

	return 1;
}

sub run {
	my $self = shift;

	# Download the needed modules
	foreach my $name ( $self->names ) {
		next if $name =~ /(\.par|[\d.]+)$/;
		next if $self->{dists}->{$name};
		$self->_fetch_uri($name);
	}

	# Inject them into CPAN and install
	foreach my $name ( $self->names ) {
		# Install via PAR::Dist
		if ( $name =~ /(\.par|[\d.]+)$/ ) {
			$self->_par_install($name);
			next;
		}

		# Install via CPAN.pm
		$self->_cpan_inject($name);
		$self->_cpan_install($name);
	}

	return 1;
}

1;

=pod

=head1 SUPPORT

See the main L<pip> module for support information.

=head1 AUTHORS

Adam Kennedy E<lt>adamk@cpan.orgE<gt>

=head1 SEE ALSO

L<pip>, L<Module::Plan>

=head1 COPYRIGHT

Copyright 2006 - 2010 Adam Kennedy.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut
