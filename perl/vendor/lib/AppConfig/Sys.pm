#============================================================================
#
# AppConfig::Sys.pm
#
# Perl5 module providing platform-specific information and operations as 
# required by other AppConfig::* modules.
#
# Written by Andy Wardley <abw@wardley.org>
#
# Copyright (C) 1997-2003 Andy Wardley.  All Rights Reserved.
# Copyright (C) 1997,1998 Canon Research Centre Europe Ltd.
#
# $Id: Sys.pm,v 1.61 2004/02/04 10:11:23 abw Exp $
#
#============================================================================

package AppConfig::Sys;
use strict;
use warnings;
use POSIX qw( getpwnam getpwuid );

our $VERSION = '1.65';
our ($AUTOLOAD, $OS, %CAN, %METHOD);


BEGIN {
    # define the methods that may be available
    if($^O =~ m/win32/i) {
        $METHOD{ getpwuid } = sub { 
            return wantarray() 
                ? ( (undef) x 7, getlogin() )
                : getlogin(); 
        };
        $METHOD{ getpwnam } = sub { 
            die("Can't getpwnam on win32"); 
        };
    }
    else
    {
        $METHOD{ getpwuid } = sub { 
            getpwuid( defined $_[0] ? shift : $< ); 
        };
        $METHOD{ getpwnam } = sub { 
            getpwnam( defined $_[0] ? shift : '' );
        };
    }
    
    # try out each METHOD to see if it's supported on this platform;
    # it's important we do this before defining AUTOLOAD which would
    # otherwise catch the unresolved call
    foreach my $method  (keys %METHOD) {
        eval { &{ $METHOD{ $method } }() };
    	$CAN{ $method } = ! $@;
    }
}



#------------------------------------------------------------------------
# new($os)
#
# Module constructor.  An optional operating system string may be passed
# to explicitly define the platform type.
#
# Returns a reference to a newly created AppConfig::Sys object.
#------------------------------------------------------------------------

sub new {
    my $class = shift;
    
    my $self = {
        METHOD => \%METHOD,
        CAN    => \%CAN,
    };

    bless $self, $class;

    $self->_configure(@_);
	
    return $self;
}


#------------------------------------------------------------------------
# AUTOLOAD
#
# Autoload function called whenever an unresolved object method is 
# called.  If the method name relates to a METHODS entry, then it is 
# called iff the corresponding CAN_$method is set true.  If the 
# method name relates to a CAN_$method value then that is returned.
#------------------------------------------------------------------------

sub AUTOLOAD {
    my $self = shift;
    my $method;


    # splat the leading package name
    ($method = $AUTOLOAD) =~ s/.*:://;

    # ignore destructor
    $method eq 'DESTROY' && return;

    # can_method()
    if ($method =~ s/^can_//i && exists $self->{ CAN }->{ $method }) {
        return $self->{ CAN }->{ $method };
    }
    # method() 
    elsif (exists $self->{ METHOD }->{ $method }) {
        if ($self->{ CAN }->{ $method }) {
            return &{ $self->{ METHOD }->{ $method } }(@_);
        }
        else {
            return undef;
        }
    } 
    # variable
    elsif (exists $self->{ uc $method }) {
        return $self->{ uc $method };
    }
    else {
        warn("AppConfig::Sys->", $method, "(): no such method or variable\n");
    }

    return undef;
}


#------------------------------------------------------------------------
# _configure($os)
#
# Uses the first parameter, $os, the package variable $AppConfig::Sys::OS,
# the value of $^O, or as a last resort, the value of
# $Config::Config('osname') to determine the current operating
# system/platform.  Sets internal variables accordingly.
#------------------------------------------------------------------------

sub _configure {
    my $self = shift;

    # operating system may be defined as a parameter or in $OS
    my $os = shift || $OS;


    # - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    # The following was lifted (and adapated slightly) from Lincoln Stein's 
    # CGI.pm module, version 2.36...
    #
    # FIGURE OUT THE OS WE'RE RUNNING UNDER
    # Some systems support the $^O variable.  If not
    # available then require() the Config library
    unless ($os) {
	unless ($os = $^O) {
	    require Config;
	    $os = $Config::Config{'osname'};
	}
    }
    if ($os =~ /win32/i) {
        $os = 'WINDOWS';
    } elsif ($os =~ /vms/i) {
        $os = 'VMS';
    } elsif ($os =~ /mac/i) {
        $os = 'MACINTOSH';
    } elsif ($os =~ /os2/i) {
        $os = 'OS2';
    } else {
        $os = 'UNIX';
    }


    # The path separator is a slash, backslash or semicolon, depending
    # on the platform.
    my $ps = {
        UNIX      => '/',
        OS2       => '\\',
        WINDOWS   => '\\',
        MACINTOSH => ':',
        VMS       => '\\'
    }->{ $os };
    #
    # Thanks Lincoln!
    # - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 


    $self->{ OS      } = $os;
    $self->{ PATHSEP } = $ps;
}


#------------------------------------------------------------------------
# _dump()
#
# Dump internals for debugging.
#------------------------------------------------------------------------

sub _dump {
    my $self = shift;

    print "=" x 71, "\n";
    print "Status of AppConfig::Sys (Version $VERSION) object: $self\n";
    print "    Operating System : ", $self->{ OS      }, "\n";
    print "      Path Separator : ", $self->{ PATHSEP }, "\n";
    print "   Available methods :\n";
    foreach my $can (keys %{ $self->{ CAN } }) {
        printf "%20s : ", $can;
        print  $self->{ CAN }->{ $can } ? "yes" : "no", "\n";
    }
    print "=" x 71, "\n";
}



1;

__END__

=pod

=head1 NAME

AppConfig::Sys - Perl5 module defining platform-specific information and methods for other AppConfig::* modules.

=head1 SYNOPSIS

    use AppConfig::Sys;
    my $sys = AppConfig::Sys->new();

    @fields = $sys->getpwuid($userid);
    @fields = $sys->getpwnam($username);

=head1 OVERVIEW

AppConfig::Sys is a Perl5 module provides platform-specific information and
operations as required by other AppConfig::* modules.

AppConfig::Sys is distributed as part of the AppConfig bundle.

=head1 DESCRIPTION

=head2 USING THE AppConfig::Sys MODULE

To import and use the AppConfig::Sys module the following line should
appear in your Perl script:

     use AppConfig::Sys;

AppConfig::Sys is implemented using object-oriented methods.  A new
AppConfig::Sys object is created and initialised using the
AppConfig::Sys->new() method.  This returns a reference to a new
AppConfig::Sys object.  

    my $sys = AppConfig::Sys->new();

This will attempt to detect your operating system and create a reference to
a new AppConfig::Sys object that is applicable to your platform.  You may 
explicitly specify an operating system name to override this automatic 
detection:

    $unix_sys = AppConfig::Sys->new("Unix");

Alternatively, the package variable $AppConfig::Sys::OS can be set to an
operating system name.  The valid operating system names are: Win32, VMS,
Mac, OS2 and Unix.  They are not case-specific.

=head2 AppConfig::Sys METHODS

AppConfig::Sys defines the following methods:

=over 4

=item getpwnam()

Calls the system function getpwnam() if available and returns the result.
Returns undef if not available.  The can_getpwnam() method can be called to
determine if this function is available.

=item getpwuid()

Calls the system function getpwuid() if available and returns the result.
Returns undef if not available.  The can_getpwuid() method can be called to
determine if this function is available.

=item 

=back

=head1 AUTHOR

Andy Wardley, E<lt>abw@wardley.orgE<gt>

=head1 COPYRIGHT

Copyright (C) 1997-2007 Andy Wardley.  All Rights Reserved.

Copyright (C) 1997,1998 Canon Research Centre Europe Ltd.

This module is free software; you can redistribute it and/or modify it under 
the term of the Perl Artistic License.

=head1 SEE ALSO

AppConfig, AppConfig::File

=cut
