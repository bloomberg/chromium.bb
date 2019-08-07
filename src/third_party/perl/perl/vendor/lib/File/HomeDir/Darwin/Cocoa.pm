package File::HomeDir::Darwin::Cocoa;

use 5.00503;
use strict;
use Cwd                   ();
use Carp                  ();
use File::HomeDir::Darwin ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '0.99';
	@ISA     = 'File::HomeDir::Darwin';

	# Load early if in a forking environment and we have
	# prefork, or at run-time if not.
	local $@;
	eval "use prefork 'Mac::SystemDirectory'";
}





#####################################################################
# Current User Methods

sub my_home {
	my $class = shift;

	# A lot of unix people and unix-derived tools rely on
	# the ability to overload HOME. We will support it too
	# so that they can replace raw HOME calls with File::HomeDir.
	if ( exists $ENV{HOME} and defined $ENV{HOME} ) {
		return $ENV{HOME};
	}

	require Mac::SystemDirectory;
	return Mac::SystemDirectory::HomeDirectory();
}

# from 10.4
sub my_desktop {
	my $class = shift;

	require Mac::SystemDirectory;
	eval {
		$class->_find_folder(Mac::SystemDirectory::NSDesktopDirectory())
	}
	||
	$class->SUPER::my_desktop;
}

# from 10.2
sub my_documents {
	my $class = shift;

	require Mac::SystemDirectory;
	eval {
		$class->_find_folder(Mac::SystemDirectory::NSDocumentDirectory())
	}
	||
	$class->SUPER::my_documents;
}

# from 10.4
sub my_data {
	my $class = shift;

	require Mac::SystemDirectory;
	eval {
		$class->_find_folder(Mac::SystemDirectory::NSApplicationSupportDirectory())
	}
	||
	$class->SUPER::my_data;
}

# from 10.6
sub my_music {
	my $class = shift;

	require Mac::SystemDirectory;
	eval {
		$class->_find_folder(Mac::SystemDirectory::NSMusicDirectory())
	}
	||
	$class->SUPER::my_music;
}

# from 10.6
sub my_pictures {
	my $class = shift;

	require Mac::SystemDirectory;
	eval {
		$class->_find_folder(Mac::SystemDirectory::NSPicturesDirectory())
	}
	||
	$class->SUPER::my_pictures;
}

# from 10.6
sub my_videos {
	my $class = shift;

	require Mac::SystemDirectory;
	eval {
		$class->_find_folder(Mac::SystemDirectory::NSMoviesDirectory())
	}
	||
	$class->SUPER::my_videos;
}

sub _find_folder {
	my $class = shift;
	my $name  = shift;

	require Mac::SystemDirectory;
	my $folder = Mac::SystemDirectory::FindDirectory($name);
	return undef unless defined $folder;

	unless ( -d $folder ) {
		# Make sure that symlinks resolve to directories.
		return undef unless -l $folder;
		my $dir = readlink $folder or return;
		return undef unless -d $dir;
	}

	return Cwd::abs_path($folder);
}

1;

=pod

=head1 NAME

File::HomeDir::Darwin::Cocoa - Find your home and other directories on Darwin (OS X)

=head1 DESCRIPTION

This module provides Darwin-specific implementations for determining
common user directories using Cocoa API through
L<Mac::SystemDirectory>.  In normal usage this module will always be
used via L<File::HomeDir>.

Theoretically, this should return the same paths as both of the other
Darwin drivers.

Because this module requires L<Mac::SystemDirectory>, if the module
is not installed, L<File::HomeDir> will fall back to L<File::HomeDir::Darwin>.

=head1 SYNOPSIS

  use File::HomeDir;
  
  # Find directories for the current user
  $home    = File::HomeDir->my_home;      # /Users/mylogin
  $desktop = File::HomeDir->my_desktop;   # /Users/mylogin/Desktop
  $docs    = File::HomeDir->my_documents; # /Users/mylogin/Documents
  $music   = File::HomeDir->my_music;     # /Users/mylogin/Music
  $pics    = File::HomeDir->my_pictures;  # /Users/mylogin/Pictures
  $videos  = File::HomeDir->my_videos;    # /Users/mylogin/Movies
  $data    = File::HomeDir->my_data;      # /Users/mylogin/Library/Application Support

=cut
