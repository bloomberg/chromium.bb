# $Id: Cmd.pm,v 1.1 2007/10/23 12:18:37 pkaluski Exp $
=head1 NAME

Win32::GuiTest::Cmd - Perl Batch File Enhancer. Part of Win32::GuiTest.

=head1 SYNOPSIS

  use Win32::GuiTest::Cmd ':ASK';

  Pause("Press ENTER to start the setup...");

  setup_network() 
    if YesOrNo("Setup networking component?");

  $address = AskForIt("What's your new ip address?", 
    "122.122.122.122");

  $dir = AskForDir("Where should I put the new files?", 
    "c:\\temp");

  copy_files($dir) if $dir;

  $exe = AskForExe("Where is your net setup program?", 
    "/foo/bar.exe");

  system($exe) if YesOrNo("Want me to run the net setup?");


=head1 DESCRIPTION

Instead of writing batch files (although on NT they are almost
usable), I've resorted more and more to writing Perl scripts for
common sysadmin/build/test chores. This module makes that kind of
thing easier.

Other modules I've found useful for that kind of work:

C<use Win32::NetAdmin;>

C<use Win32::NetResource;>

C<use Win32::ODBC;>

C<use Socket;>

C<use Sys::Hostname;>

C<use File::Path  'mkpath';>

C<use Getopt::Std 'getopts';>

=cut

package Win32::GuiTest::Cmd;

require Exporter;

use vars qw(@ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);

use ExtUtils::MakeMaker;
use Cwd;
use File::Basename;

use strict;
use warnings;


@ISA = qw(Exporter);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = ();

%EXPORT_TAGS=(
    CPL => [ qw(
        Accessibility AppWizard Console DateTime Display Exchange FindFast
        Internet Joystick Modem Mouse Multimedia Network Odbc Pcmcia Ports Ras
        Regional Server System Telephony Ups Users
    )],
    ASK => [ qw(
        Pause YesOrDie YesOrNo AskForIt IsExe AskForExe AskForDir AskAndRun
    )],
    REG => [ qw(
        RegisterCom UnregisterCom AddToRegistry
    )],
    MISC => [ qw(
        WhichExe TempFileName
    )],
);

@EXPORT_OK= ();
{ my $ref;
    foreach $ref (  values(%EXPORT_TAGS)  ) {
        push( @EXPORT_OK, @$ref );
    }
}
$EXPORT_TAGS{ALL}= \@EXPORT_OK;

# Preloaded methods go here.

=head1 FUNCTIONS


=head2 Console

Console interaction functions heavily based on the command-line installer for 
the libwin32 distribution written by Gurusamy Sarathy.

=over 4

=item Pause([$message])

Shows a message and waits until the user presses ENTER.

=cut

sub Pause {
    my $msj = shift;
    print "$msj" if $msj; 
    scalar(<STDIN>); # hang around in case they ran it from Explorer
}

=item YesOrDie([$message])

Asks for a [y/n] response using the message you specify. The program
dies if you answer 'n'.

=cut

sub YesOrDie {
    my $m = shift || "Proceed?";
    print "$m [y] ";
    die "Bailing out\n" if scalar(<STDIN>) !~ /^\s*(y|$)/i;
}

=item YesOrNo([$msg])

Asks for a [y/n] response using the message you specify. Returns 1 if
you type 'y' or 0 otherwise.

=cut

sub YesOrNo {
    my $m = shift || "Which?";
    print "$m [y] ";
    return 1 if scalar(<STDIN>) =~ /^\s*(y|$)/i;
}

=item AskForIt([$question],[$def_value])

Asks the user to input a value and returns it. If you omit $question
a default question will be used. If you omit $def_value, false will be used
as default return value.

=cut

sub AskForIt {
    my $m = shift || "Enter value";
    my $def = shift;
    $def = "" unless defined $def;
    print "$m \[$def\] ";
    my $v = <STDIN>;
    chomp $v;
    return ($v =~ /^\s*$/ ? $def : $v);
}

=item IsExe($filename)

Checks if a file is executable.

=cut

sub IsExe {
    my $exe = shift;
    return $exe	if -x $exe && ! -d _;
}

=item AskForExe([$question],[$def_exe])

Just like AskForIt, but returns false if the value 
is not an executable file.

=cut

sub AskForExe {
    my $exe = AskForIt(@_); 
    return $exe if -x $exe && ! -d _;
    warn "$exe is not executable\n";
    return "";
}

=item AskForDir([$question],[$def_dir])

Just like AskForIt, but returns false if the value 
is not a directory.

=cut

sub AskForDir {
    my $dir = AskForIt(@_); 
    return $dir if -d $dir;
    warn "$dir is not a directory\n";
    return "";
}

=item AskAndRun([$question],[$def_exe])

Asks for an exe file an runs it using C<system>.

=cut

sub AskAndRun {
    my $exe = AskForExe(@_);
    system($exe) if $exe; 
}

sub try_again {
    print "Let's try this again.\n";
    goto shift;
}

=back

=head2 System Configuration

Mostly allow opening Win32 Control Panel Applets programatically.

=over 4

=item RunCpl($applet)

Opens a Control Panel Applet (.cpl) by name.

 RunCpl("modem.cpl");

=cut

sub RunCpl {
    my $cpl = shift;
    system("start rundll32.exe shell32.dll,Control_RunDLL $cpl");
}

=item Modem, Network, Console, Accessibility, AppWizard, Pcmcia,
    Regional, Joystick, Mouse, Multimedia, Odbc, Ports, Server,
    System, Telephony, DateTime, Ups, Internet, Display, FindFast,
    Exchange, 3ComPace

Each of them opens the corresponding Control Panel Applet.

=cut

sub Modem         { RunCpl "modem.cpl";    }
sub Network       { RunCpl "ncpa.cpl";     }
sub Console       { RunCpl "console.cpl";  }
sub Accessibility { RunCpl "access.cpl"; }
sub AppWizard     { RunCpl "appwiz.cpl"; }
sub Pcmcia        { RunCpl "DEVAPPS.cpl"; }
sub Regional      { RunCpl "intl.cpl"; }
sub Joystick      { RunCpl "joy.cpl"; }
sub Mouse         { RunCpl "main.cpl"; }
sub Multimedia    { RunCpl "mmsys.cpl"; }
sub Odbc          { RunCpl "ODBCCP32.cpl"; }
sub Ports         { RunCpl "PORTS.cpl"; }
sub Server        { RunCpl "srvmgr.cpl"; }
sub System        { RunCpl "sysdm.cpl"; }
sub Telephony     { RunCpl "telephon.cpl"; }
sub DateTime      { RunCpl "timedate.cpl"; }
sub Ups           { RunCpl "ups.cpl"; }
sub Internet      { RunCpl "INETCPL.cpl"; }
sub Display       { RunCpl "DESK.cpl"; }

# Propietary CPL applets
sub FindFast      { RunCpl "FINDFAST.cpl"; }
sub Exchange      { RunCpl "MLCFG32.cpl"; }
#sub 3ComPace      { RunCpl "pacecfg.cpl"; }

# Some useful system utilities

=item Ras

Installs or configures the RAS (Remote Access Service) component.

=cut

sub Ras     { system("start rasphone.exe"); }

=item Users

Runs the User/Group Manager application.

=cut

sub Users   { system("start musrmgr.exe"); }

=back

=head2 Registry

Manipulate the registry.

=over 4

=item RegisterCom($path)

Uses regsvr32.exe to register a COM server.

  RegisterCom("c:\\myfiles\\mycontrol.ocx");

=cut

sub RegisterCom {
    my $server = shift;
    system("regsvr32 $server") && 
        warn "Could not register server $server\n";
}

=item UnregisterCom($path)

Uses regsvr32.exe to unregister a COM server.

  UnregisterCom("c:\\myfiles\\mycontrol.ocx");

=cut

sub UnregisterCom {
    my $server = shift;
    system("regsvr32 /u $server");
}

=item AddToRegistry($regfile)

Uses regedit.exe to merge a .reg file into the system registry.

  AddToRegistry("c:\\myfiles\\test.reg");

=cut

sub AddToRegistry {
    my $reg = shift;
    system("regedit $reg");
}

=back

=head2 Misc

Sorry about that...

=over 4

=item WhichExe($file)

Takes a command name guesses which 
executable file gets executed if you invoke the command.

    WhichExe("regedit")  -> C:\WINNT\regedit.exe
    WhichExe("regsvr32") -> D:\bin\regsvr32.exe
    WhichExe("ls")       -> D:\Usr\Cygnus\B19\H-i386-cygwin32\bin\ls.exe

Based on original code grabbed from CPAN::FirstTime.

Added support for NT file extension associations:

   WhichExe("test.pl")   -> perl D:\SCRIPTS\test.pl %*
   WhichExe("report.ps") -> D:\gstools\gsview\gsview32.exe D:\TMP\report.ps

=cut

#
# Uses extensions in PATHEXT or default extensions to look for
# posible filenames.
#
sub MaybeCommand {
    my $file = shift;
    for (split(/;/, $ENV{'PATHEXT'} || '.COM;.EXE;.BAT;.CMD')) {
		return "${file}$_" if -e "${file}$_";
    }
    return;
}

sub MaybeAssoc {
    my $abs = shift;
    my($name,$path,$suffix) = fileparse($abs, '\..*');
    return unless $suffix;
    for (`assoc`) {
	chomp;
	if (/$suffix=(.*)/) {
	    my $type = $1;
	    for (`ftype`) {
		chomp;
		if (/$type=(.*)/) {
		    my $cmdline = $1;
		    my $count = ($cmdline =~ s/%1/${abs}/); 
		    $cmdline =~ s/%\*/${abs}/ if !$count;
		    return $cmdline;    
		}
	    }
	    #warn "No ftype for $type\n";	
	}
    }
    return;
}

sub WhichExe {
    my(@path) = split /$Config::Config{'path_sep'}/, $ENV{'PATH'};
    my $exe = shift;
    my $path = shift || [ @path ];
    unshift @$path, getcwd; # Don't forget to check the cwd first

    #warn "in WhichExe exe[$exe] path[@$path]";
    for (@$path) {
    	my $abs = MM->catfile($_, $exe);
        my $ret;
	# Try with file associations
	if (($ret = MaybeAssoc($abs))) {
            return $ret;
	}
	# Try with command extensions
        if (($ret = MaybeCommand($abs))) {
            return $ret;
	}
    }
}

=item TempFileName

Returns the full path for a temporary file that will not collide with an
existing file.

=cut

sub TempFileName {
    my $pre = "aaaaaaaa";
    for (0..10000000) {
       my $name = MM->catfile("$ENV{TMP}", "$pre.reg");
       return $name unless -e $name;
       $pre++;
    }
    return; 
}

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__

=back

=head1 AUTHOR

Ernesto Guisado E<lt>erngui@acm.orgE<gt>, E<lt>http://triumvir.orgE<gt>

=cut
