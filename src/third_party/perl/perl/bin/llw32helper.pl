#!perl

use 5.008001;
use strict;
use warnings;
use local::lib 1.004007 qw();
use English qw(-no_match_vars);
use Carp qw(croak);
use IO::Interactive qw(is_interactive);
use File::HomeDir 0.81;
use File::Spec::Functions 3.2701 qw(splitpath catpath catdir);
use File::Path qw(mkpath);
use Win32::TieRegistry 0.26 qw(:KEY_);

our $VERSION = our $VERSION_STR = '0.990';
$VERSION =~ s/_//sm;

print <<"__END_TEXT__";
llwin32helper version $VERSION_STR

Configures local::lib on Windows.

Run "perldoc App::local::lib::Win32Helper" for more information, including
the license and copyright.

__END_TEXT__


if ( !is_interactive() ) {
	croak 'llw32helper must be run interactively.';
}

# Access = KEY_READ | KEY_WRITE | KEY_WOW64_64KEY (0x100 = 256)
my $environment_obj = Win32::TieRegistry->new(
	'CUser/Environment/',
	{   Delimiter => q{/},
		Access    => KEY_READ() | KEY_WRITE() | 256
	}
) or croak "Can't access HKEY_CURRENT_USER subkey: $EXTENDED_OS_ERROR\n";

$environment_obj->FastDelete(1);
$environment_obj->ArrayValues(0);

my $environment_key = $environment_obj->TiedRef();

local $ENV{HOME} = undef;

my $ll_exists = 0;
my $default_path;
if ( exists $environment_key->{'/PERL_MB_OPT'} ) {
	$ll_exists = 1;
	my ( $volume, $directories, $file ) =
	  File::Spec->splitpath( $environment_key->{'PERL_MB_OPT'} );
	$default_path = catpath( $volume, $directories, undef );
} else {
	$default_path =
	  catdir( Win32::GetShortPathName( File::HomeDir->my_home() ),
		'perl5' );
}

if ($ll_exists) {
  EXISTS:
	print
"Do you wish to remove the local::lib settings from $default_path? [y/N] ";

	my $answer = <>;
	chomp $answer if defined $answer;

	$answer = 'n' if $answer eq q{};

	if ( 'n' eq lc substr $answer, 0, 1 ) {
		print "llwin32helper exiting.\n";
		exit;
	}

	goto EXISTS if ( 'y' ne lc substr $answer, 0, 1 );

	delete $environment_key->{'/PERL_MB_OPT'};
	delete $environment_key->{'/PERL_MM_OPT'};
	delete $environment_key->{'/PERL5LIB'};

	if ( $environment_key->{'/PATH'} =~ m/;/smx ) {
		my $dir = catdir( $default_path, 'bin' );
		$dir =~ s{\\}{\\\\}gsmx;
		my $path = $environment_key->{'/PATH'};
		$path =~ s{$dir;}{}gsmx;
		$environment_key->{'/PATH'} = $path;
	} else {
		delete $environment_key->{'/PATH'};
	}

	print <<"__END_TEXT__";

llwin32helper has removed the environment entries that make CPAN/CPANPLUS 
install future modules to $default_path.

This script does not delete the files in that directory, however.
(You may wish to delete them yourself.)

You'll need to reboot for the changes to register.
__END_TEXT__

	exit;

} else {
  NOTEXISTS:
	print 'Do you wish to install future modules in a local area? [y/N] ';

	my $answer = <>;
	chomp $answer if defined $answer;

	$answer = 'n' if $answer eq q{};

	if ( 'n' eq lc substr $answer, 0, 1 ) {
		print "llwin32helper exiting.\n";
		exit;
	}

	goto NOTEXISTS if ( 'y' ne lc substr $answer, 0, 1 );

  PATH:
	print "\nWhere do you want to install modules? [$default_path] ";

	$answer = <>;
	chomp $answer if defined $answer;

	$answer = $default_path if $answer eq q{};

	if ( !-d $answer ) {
		print "Creating path.\n";
		mkpath($answer);
	}

	my %ll_env_entries =
	  'local::lib'->build_environment_vars_for( $answer, 0 );

	$environment_key->{'/PERL_MB_OPT'}   = $ll_env_entries{'PERL_MB_OPT'};
	$environment_key->{'/PERL_MM_OPT'}   = $ll_env_entries{'PERL_MM_OPT'};
	$environment_key->{'/PERL5LIB'}      = $ll_env_entries{'PERL5LIB'};

	if ( exists $environment_key->{'/PATH'} ) {
		$environment_key->{'/PATH'} = join q{;},
		  'local::lib'->install_base_bin_path($answer),
		  $environment_key->{'/PATH'};
	} else {
		$environment_key->{'/PATH'} =
		  local::lib->install_base_bin_path($answer);
	}

	'local::lib'->ensure_dir_structure_for($answer);

	print <<"__END_TEXT__";

llwin32helper has added environment entries and files so that CPAN/CPANPLUS 
installs future modules to $answer.

To use modules installed this way in your scripts, insert this line:
    use local::lib '~\\perl5'; 
(if you changed the directory, use that directory instead.)

To remove these environment entries, run llw32helper again.

You'll need to reboot for these environment variables to register.
__END_TEXT__

	exit;
} ## end else [ if ($ll_exists) ]
