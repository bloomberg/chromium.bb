#============================================================================
#
# AppConfig::Args.pm
#
# Perl5 module to read command line argument and update the variable 
# values in an AppConfig::State object accordingly.
#
# Written by Andy Wardley <abw@wardley.org>
#
# Copyright (C) 1997-2007 Andy Wardley.  All Rights Reserved.
# Copyright (C) 1997,1998 Canon Research Centre Europe Ltd.
#============================================================================

package AppConfig::Args;
use strict;
use warnings;
use AppConfig::State;
our $VERSION = '1.65';


#------------------------------------------------------------------------
# new($state, \@args)
#
# Module constructor.  The first, mandatory parameter should be a 
# reference to an AppConfig::State object to which all actions should 
# be applied.  The second parameter may be a reference to a list of 
# command line arguments.  This list reference is passed to args() for
# processing.
#
# Returns a reference to a newly created AppConfig::Args object.
#------------------------------------------------------------------------

sub new {
    my $class = shift;
    my $state = shift;
    

    my $self = {
        STATE    => $state,                # AppConfig::State ref
        DEBUG    => $state->_debug(),      # store local copy of debug
        PEDANTIC => $state->_pedantic,     # and pedantic flags
    };

    bless $self, $class;
        
    # call parse() to parse any arg list passed 
    $self->parse(shift)
        if @_;

    return $self;
}


#------------------------------------------------------------------------
# parse(\@args)
#
# Examines the argument list and updates the contents of the 
# AppConfig::State referenced by $self->{ STATE } accordingly.  If 
# no argument list is provided then the method defaults to examining 
# @ARGV.  The method reports any warning conditions (such as undefined
# variables) by calling $self->{ STATE }->_error() and then continues to
# examine the rest of the list.  If the PEDANTIC option is set in the
# AppConfig::State object, this behaviour is overridden and the method
# returns 0 immediately on any parsing error.
#
# Returns 1 on success or 0 if one or more warnings were raised.
#------------------------------------------------------------------------

sub parse {
    my $self = shift;
    my $argv = shift || \@ARGV;
    my $warnings = 0;
    my ($arg, $nargs, $variable, $value);


    # take a local copy of the state to avoid much hash dereferencing
    my ($state, $debug, $pedantic) = @$self{ qw( STATE DEBUG PEDANTIC ) };

    # loop around arguments
    ARG: while (@$argv && $argv->[0] =~ /^-/) {
        $arg = shift(@$argv);

        # '--' indicates the end of the options
        last if $arg eq '--';

        # strip leading '-';
        ($variable = $arg) =~ s/^-(-)?//;

        # test for '--' prefix and push back any '=value' item
        if (defined $1) {
            ($variable, $value) = split(/=/, $variable);
            unshift(@$argv, $value) if defined $value;
        }

        # check the variable exists
        if ($state->_exists($variable)) {

            # see if it expects any mandatory arguments
            $nargs = $state->_argcount($variable);
            if ($nargs) {
                # check there's another arg and it's not another '-opt'
                if(defined($argv->[0])) {
                    $value = shift(@$argv);
                }
                else {
                    $state->_error("$arg expects an argument");
                    $warnings++;
                    last ARG if $pedantic;
                    next;
                }
            }
            else {
                # set a value of 1 if option doesn't expect an argument
                $value = 1;
            }

            # set the variable with the new value
            $state->set($variable, $value);
        }
        else {
            $state->_error("$arg: invalid option");
            $warnings++;
            last ARG if $pedantic;
        }
    }

    # return status
    return $warnings ? 0 : 1;
}



1;

__END__

=head1 NAME

AppConfig::Args - Perl5 module for reading command line arguments.

=head1 SYNOPSIS

    use AppConfig::Args;

    my $state   = AppConfig::State->new(\%cfg);
    my $cfgargs = AppConfig::Args->new($state);

    $cfgargs->parse(\@args);            # read args

=head1 OVERVIEW

AppConfig::Args is a Perl5 module which reads command line arguments and 
uses the options therein to update variable values in an AppConfig::State 
object.

AppConfig::File is distributed as part of the AppConfig bundle.

=head1 DESCRIPTION

=head2 USING THE AppConfig::Args MODULE

To import and use the AppConfig::Args module the following line should appear
in your Perl script:

    use AppConfig::Args;

AppConfig::Args is used automatically if you use the AppConfig module 
and create an AppConfig::Args object through the parse() method.
      
AppConfig::File is implemented using object-oriented methods.  A new 
AppConfig::Args object is created and initialised using the new() method.
This returns a reference to a new AppConfig::File object.  A reference to
an AppConfig::State object should be passed in as the first parameter:
       
    my $state   = AppConfig::State->new();
    my $cfgargs = AppConfig::Args->new($state);

This will create and return a reference to a new AppConfig::Args object. 

=head2 PARSING COMMAND LINE ARGUMENTS

The C<parse()> method is used to read a list of command line arguments and 
update the STATE accordingly.  A reference to the list of arguments should
be passed in.

    $cfgargs->parse(\@ARGV);

If the method is called without a reference to an argument list then it
will examine and manipulate @ARGV.

If the PEDANTIC option is turned off in the AppConfig::State object, any 
parsing errors (invalid variables, unvalidated values, etc) will generate
warnings, but not cause the method to return.  Having processed all
arguments, the method will return 1 if processed without warning or 0 if
one or more warnings were raised.  When the PEDANTIC option is turned on,
the method generates a warning and immediately returns a value of 0 as soon
as it encounters any parsing error.

The method continues parsing arguments until it detects the first one that
does not start with a leading dash, '-'.  Arguments that constitute values
for other options are not examined in this way.

=head1 FUTURE DEVELOPMENT

This module was developed to provide backwards compatibility (to some 
degree) with the preceeding App::Config module.  The argument parsing 
it provides is basic but offers a quick and efficient solution for those
times when simple option handling is all that is required.

If you require more flexibility in parsing command line arguments, then 
you should consider using the AppConfig::Getopt module.  This is loaded 
and used automatically by calling the AppConfig getopt() method.

The AppConfig::Getopt module provides considerably extended functionality 
over the AppConfig::Args module by delegating out the task of argument 
parsing to Johan Vromans' Getopt::Long module.  For advanced command-line 
parsing, this module (either Getopt::Long by itself, or in conjunction with 
AppConfig::Getopt) is highly recommended.

=head1 AUTHOR

Andy Wardley, E<lt>abw@wardley.orgE<gt>

=head1 COPYRIGHT

Copyright (C) 1997-2007 Andy Wardley.  All Rights Reserved.

Copyright (C) 1997,1998 Canon Research Centre Europe Ltd.

This module is free software; you can redistribute it and/or modify it 
under the same terms as Perl itself.

=head1 SEE ALSO

AppConfig, AppConfig::State, AppConfig::Getopt, Getopt::Long

=cut
