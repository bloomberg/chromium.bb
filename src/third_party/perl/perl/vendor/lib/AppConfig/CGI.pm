#============================================================================
#
# AppConfig::CGI.pm
#
# Perl5 module to provide a CGI interface to AppConfig.  Internal variables
# may be set through the CGI "arguments" appended to a URL.
# 
# Written by Andy Wardley <abw@wardley.org>
#
# Copyright (C) 1997-2003 Andy Wardley.  All Rights Reserved.
# Copyright (C) 1997,1998 Canon Research Centre Europe Ltd.
#
#============================================================================

package AppConfig::CGI;
use strict;
use warnings;
use AppConfig::State;
our $VERSION = '1.65';


#------------------------------------------------------------------------
# new($state, $query)
#
# Module constructor.  The first, mandatory parameter should be a 
# reference to an AppConfig::State object to which all actions should 
# be applied.  The second parameter may be a string containing a CGI
# QUERY_STRING which is then passed to parse() to process.  If no second
# parameter is specifiied then the parse() process is skipped.
#
# Returns a reference to a newly created AppConfig::CGI object.
#------------------------------------------------------------------------

sub new {
    my $class = shift;
    my $state = shift;
    my $self  = {
        STATE    => $state,                # AppConfig::State ref
        DEBUG    => $state->_debug(),      # store local copy of debug
        PEDANTIC => $state->_pedantic,     # and pedantic flags
    };
    bless $self, $class;
        
    # call parse(@_) to parse any arg list passed 
    $self->parse(@_)
        if @_;

    return $self;
}


#------------------------------------------------------------------------
# parse($query)
#
# Method used to parse a CGI QUERY_STRING and set internal variable 
# values accordingly.  If a query is not passed as the first parameter,
# then _get_cgi_query() is called to try to determine the query by 
# examing the environment as per CGI protocol.
#
# Returns 0 if one or more errors or warnings were raised or 1 if the
# string parsed successfully.
#------------------------------------------------------------------------

sub parse {
    my $self     = shift;
    my $query    = shift;
    my $warnings = 0;
    my ($variable, $value, $nargs);
    

    # take a local copy of the state to avoid much hash dereferencing
    my ($state, $debug, $pedantic) = @$self{ qw( STATE DEBUG PEDANTIC ) };

    # get the cgi query if not defined
    $query = $ENV{ QUERY_STRING }
        unless defined $query;

    # no query to process
    return 1 unless defined $query;

    # we want to install a custom error handler into the AppConfig::State 
    # which appends filename and line info to error messages and then 
    # calls the previous handler;  we start by taking a copy of the 
    # current handler..
    my $errhandler = $state->_ehandler();

    # install a closure as a new error handler
    $state->_ehandler(
        sub {
            # modify the error message 
            my $format  = shift;
            $format =~ s/</&lt;/g;
            $format =~ s/>/&gt;/g;
            $format  = "<p>\n<b>[ AppConfig::CGI error: </b>$format<b> ] </b>\n<p>\n";
            # send error to stdout for delivery to web client
            printf($format, @_);
        }
    );


    PARAM: foreach (split('&', $query)) {

        # extract parameter and value from query token
        ($variable, $value) = map { _unescape($_) } split('=');

        # check an argument was provided if one was expected
        if ($nargs = $state->_argcount($variable)) {
            unless (defined $value) {
                $state->_error("$variable expects an argument");
                $warnings++;
                last PARAM if $pedantic;
                next;
            }
        }
        # default an undefined value to 1 if ARGCOUNT_NONE
        else {
            $value = 1 unless defined $value;
        }

        # set the variable, noting any error
        unless ($state->set($variable, $value)) {
            $warnings++;
            last PARAM if $pedantic;
        }
    }

    # restore original error handler
    $state->_ehandler($errhandler);

    # return $warnings => 0, $success => 1
    return $warnings ? 0 : 1;
}



# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# The following sub-routine was lifted from Lincoln Stein's CGI.pm
# module, version 2.36.  Name has been prefixed by a '_'.

# unescape URL-encoded data
sub _unescape {
    my($todecode) = @_;
    $todecode =~ tr/+/ /;       # pluses become spaces
    $todecode =~ s/%([0-9a-fA-F]{2})/pack("c",hex($1))/ge;
    return $todecode;
}

#
# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -




1;

__END__

=head1 NAME

AppConfig::CGI - Perl5 module for processing CGI script parameters.

=head1 SYNOPSIS

    use AppConfig::CGI;

    my $state = AppConfig::State->new(\%cfg);
    my $cgi   = AppConfig::CGI->new($state);

    $cgi->parse($cgi_query);
    $cgi->parse();               # looks for CGI query in environment

=head1 OVERVIEW

AppConfig::CGI is a Perl5 module which implements a CGI interface to 
AppConfig.  It examines the QUERY_STRING environment variable, or a string
passed explicitly by parameter, which represents the additional parameters
passed to a CGI query.  This is then used to update variable values in an
AppConfig::State object accordingly.

AppConfig::CGI is distributed as part of the AppConfig bundle.

=head1 DESCRIPTION

=head2 USING THE AppConfig::CGI MODULE

To import and use the AppConfig::CGI module the following line should appear
in your Perl script:

    use AppConfig::CGI;

AppConfig::CGI is used automatically if you use the AppConfig module
and create an AppConfig::CGI object through the cgi() method.
AppConfig::CGI is implemented using object-oriented methods.  A new
AppConfig::CGI object is created and initialised using the new()
method.  This returns a reference to a new AppConfig::CGI object.  A
reference to an AppConfig::State object should be passed in as the
first parameter: 

    my $state = AppConfig::State->new(); 
    my $cgi   = AppConfig::CGI->new($state);

This will create and return a reference to a new AppConfig::CGI object. 

=head2 PARSING CGI QUERIES

The C<parse()> method is used to parse a CGI query which can be specified 
explicitly, or is automatically extracted from the "QUERY_STRING" CGI 
environment variable.  This currently limits the module to only supporting 
the GET method.

See AppConfig for information about using the AppConfig::CGI
module via the cgi() method.

=head1 AUTHOR

Andy Wardley, C<E<lt>abw@wardley.org<gt>>

=head1 COPYRIGHT

Copyright (C) 1997-2007 Andy Wardley.  All Rights Reserved.

Copyright (C) 1997,1998 Canon Research Centre Europe Ltd.

This module is free software; you can redistribute it and/or modify it 
under the same terms as Perl itself.

=head1 SEE ALSO

AppConfig, AppConfig::State

=cut

