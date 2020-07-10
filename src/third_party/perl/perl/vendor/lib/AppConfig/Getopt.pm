#============================================================================
#
# AppConfig::Getopt.pm
#
# Perl5 module to interface AppConfig::* to Johan Vromans' Getopt::Long
# module.  Getopt::Long implements the POSIX standard for command line
# options, with GNU extensions, and also traditional one-letter options.
# AppConfig::Getopt constructs the necessary Getopt:::Long configuration
# from the internal AppConfig::State and delegates the parsing of command
# line arguments to it.  Internal variable values are updated by callback
# from GetOptions().
# 
# Written by Andy Wardley <abw@wardley.org>
#
# Copyright (C) 1997-2007 Andy Wardley.  All Rights Reserved.
# Copyright (C) 1997,1998 Canon Research Centre Europe Ltd.
#
#============================================================================

package AppConfig::Getopt;
use 5.006;
use strict;
use warnings;
use AppConfig::State;
use Getopt::Long 2.17;
our $VERSION = '1.71';


#------------------------------------------------------------------------
# new($state, \@args)
#
# Module constructor.  The first, mandatory parameter should be a 
# reference to an AppConfig::State object to which all actions should 
# be applied.  The second parameter may be a reference to a list of 
# command line arguments.  This list reference is passed to parse() for
# processing.
#
# Returns a reference to a newly created AppConfig::Getopt object.
#------------------------------------------------------------------------

sub new {
    my $class = shift;
    my $state = shift;
    my $self = {
        STATE => $state,
   };

    bless $self, $class;

    # call parse() to parse any arg list passed 
    $self->parse(@_)
        if @_;

    return $self;
}


#------------------------------------------------------------------------
# parse(@$config, \@args)
#
# Constructs the appropriate configuration information and then delegates
# the task of processing command line options to Getopt::Long.
#
# Returns 1 on success or 0 if one or more warnings were raised.
#------------------------------------------------------------------------

sub parse {
    my $self  = shift;
    my $state = $self->{ STATE };
    my (@config, $args, $getopt);

    local $" = ', ';

    # we trap $SIG{__WARN__} errors and patch them into AppConfig::State
    local $SIG{__WARN__} = sub {
        my $msg = shift;

        # AppConfig::State doesn't expect CR terminated error messages
        # and it uses printf, so we protect any embedded '%' chars 
        chomp($msg);
        $state->_error("%s", $msg);
    };

    # slurp all config items into @config
    push(@config, shift) while defined $_[0] && ! ref($_[0]);   

    # add debug status if appropriate (hmm...can't decide about this)
#    push(@config, 'debug') if $state->_debug();

    # next parameter may be a reference to a list of args
    $args = shift;

    # copy any args explicitly specified into @ARGV
    @ARGV = @$args if defined $args;

    # we enclose in an eval block because constructor may die()
    eval {
        # configure Getopt::Long
        Getopt::Long::Configure(@config);

        # construct options list from AppConfig::State variables
        my @opts = $self->{ STATE   }->_getopt_state();

        # DEBUG
        if ($state->_debug()) {
            print STDERR "Calling GetOptions(@opts)\n";
            print STDERR "\@ARGV = (@ARGV)\n";
        };

        # call GetOptions() with specifications constructed from the state
        $getopt = GetOptions(@opts);
    };
    if ($@) {
        chomp($@);
        $state->_error("%s", $@);
        return 0;
    }

    # udpdate any args reference passed to include only that which is left 
    # in @ARGV
    @$args = @ARGV if defined $args;

    return $getopt;
}


#========================================================================
# AppConfig::State
#========================================================================

package AppConfig::State;

#------------------------------------------------------------------------
# _getopt_state()
#
# Constructs option specs in the Getopt::Long format for each variable 
# definition.
#
# Returns a list of specification strings.
#------------------------------------------------------------------------

sub _getopt_state {
    my $self = shift;
    my ($var, $spec, $args, $argcount, @specs);

    my $linkage = sub { $self->set(@_) };

    foreach $var (keys %{ $self->{ VARIABLE } }) {
        $spec  = join('|', $var, @{ $self->{ ALIASES }->{ $var } || [ ] });

        # an ARGS value is used, if specified
        unless (defined ($args = $self->{ ARGS }->{ $var })) {
            # otherwise, construct a basic one from ARGCOUNT
            ARGCOUNT: {
                last ARGCOUNT unless 
                    defined ($argcount = $self->{ ARGCOUNT }->{ $var });

                $args = "=s",  last ARGCOUNT if $argcount eq ARGCOUNT_ONE;
                $args = "=s@", last ARGCOUNT if $argcount eq ARGCOUNT_LIST;
                $args = "=s%", last ARGCOUNT if $argcount eq ARGCOUNT_HASH;
                $args = "!";
            }
        }
        $spec .= $args if defined $args;

        push(@specs, $spec, $linkage);
    }

    return @specs;
}



1;

__END__

=head1 NAME

AppConfig::Getopt - Perl5 module for processing command line arguments via delegation to Getopt::Long.

=head1 SYNOPSIS

    use AppConfig::Getopt;

    my $state  = AppConfig::State->new(\%cfg);
    my $getopt = AppConfig::Getopt->new($state);

    $getopt->parse(\@args);            # read args

=head1 OVERVIEW

AppConfig::Getopt is a Perl5 module which delegates to Johan Vroman's
Getopt::Long module to parse command line arguments and update values 
in an AppConfig::State object accordingly.

AppConfig::Getopt is distributed as part of the AppConfig bundle.

=head1 DESCRIPTION

=head2 USING THE AppConfig::Getopt MODULE

To import and use the AppConfig::Getopt module the following line should appear
in your Perl script:

    use AppConfig::Getopt;

AppConfig::Getopt is used automatically if you use the AppConfig module 
and create an AppConfig::Getopt object through the getopt() method.

AppConfig::Getopt is implemented using object-oriented methods.  A new 
AppConfig::Getopt object is created and initialised using the new() method.
This returns a reference to a new AppConfig::Getopt object.  A reference to
an AppConfig::State object should be passed in as the first parameter:

    my $state  = AppConfig::State->new();
    my $getopt = AppConfig::Getopt->new($state);

This will create and return a reference to a new AppConfig::Getopt object. 

=head2 PARSING COMMAND LINE ARGUMENTS

The C<parse()> method is used to read a list of command line arguments and 
update the state accordingly.  

The first (non-list reference) parameters may contain a number of 
configuration strings to pass to Getopt::Long::Configure.  A reference 
to a list of arguments may additionally be passed or @ARGV is used by 
default.

    $getopt->parse();                       # uses @ARGV
    $getopt->parse(\@myargs);
    $getopt->parse(qw(auto_abbrev debug));  # uses @ARGV
    $getopt->parse(qw(debug), \@myargs);

See Getopt::Long for details of the configuartion options available.

A Getopt::Long specification string is constructed for each variable 
defined in the AppConfig::State.  This consists of the name, any aliases
and the ARGS value for the variable.

These specification string are then passed to Getopt::Long, the arguments
are parsed and the values in the AppConfig::State updated.

See AppConfig for information about using the AppConfig::Getopt
module via the getopt() method.

=head1 AUTHOR

Andy Wardley, E<lt>abw@wardley.orgE<gt>

=head1 COPYRIGHT

Copyright (C) 1997-2007 Andy Wardley.  All Rights Reserved.

Copyright (C) 1997,1998 Canon Research Centre Europe Ltd.

This module is free software; you can redistribute it and/or modify it 
under the same terms as Perl itself.

=head1 ACKNOWLEDGMENTS

Many thanks are due to Johan Vromans for the Getopt::Long module.  He was 
kind enough to offer assistance and access to early releases of his code to 
enable this module to be written.

=head1 SEE ALSO

AppConfig, AppConfig::State, AppConfig::Args, Getopt::Long

=cut
