#============================================================================
#
# AppConfig::State.pm
#
# Perl5 module in which configuration information for an application can
# be stored and manipulated.  AppConfig::State objects maintain knowledge 
# about variables; their identities, options, aliases, targets, callbacks 
# and so on.  This module is used by a number of other AppConfig::* modules.
#
# Written by Andy Wardley <abw@wardley.org>
#
# Copyright (C) 1997-2007 Andy Wardley.  All Rights Reserved.
# Copyright (C) 1997,1998 Canon Research Centre Europe Ltd.
#
#----------------------------------------------------------------------------
#
# TODO
#
# * Change varlist() to varhash() and provide another varlist() method
#   which returns a list.  Multiple parameters passed implies a hash 
#   slice/list grep, a single parameter should indicate a regex.
#
# * Perhaps allow a callback to be installed which is called *instead* of 
#   the get() and set() methods (or rather, is called by them).
#
# * Maybe CMDARG should be in there to specify extra command-line only 
#   options that get added to the AppConfig::GetOpt alias construction, 
#   but not applied in config files, general usage, etc.  The GLOBAL 
#   CMDARG might be specified as a format, e.g. "-%c" where %s = name, 
#   %c = first character, %u - first unique sequence(?).  Will 
#   GetOpt::Long handle --long to -l application automagically?
#
# * ..and an added thought is that CASE sensitivity may be required for the
#   command line (-v vs -V, -r vs -R, for example), but not for parsing 
#   config files where you may wish to treat "Name", "NAME" and "name" alike.
#
#============================================================================

package AppConfig::State;
use strict;
use warnings;

our $VERSION = '1.65';
our $DEBUG   = 0;
our $AUTOLOAD;

# need access to AppConfig::ARGCOUNT_*
use AppConfig ':argcount';

# internal per-variable hashes that AUTOLOAD should provide access to
my %METHVARS;
   @METHVARS{ qw( EXPAND ARGS ARGCOUNT ) } = ();

# internal values that AUTOLOAD should provide access to
my %METHFLAGS;
   @METHFLAGS{ qw( PEDANTIC ) } = ();

# variable attributes that may be specified in GLOBAL;
my @GLOBAL_OK = qw( DEFAULT EXPAND VALIDATE ACTION ARGS ARGCOUNT );


#------------------------------------------------------------------------
# new(\%config, @vars)
#
# Module constructor.  A reference to a hash array containing 
# configuration options may be passed as the first parameter.  This is 
# passed off to _configure() for processing.  See _configure() for 
# information about configurarion options.  The remaining parameters
# may be variable definitions and are passed en masse to define() for
# processing.
#
# Returns a reference to a newly created AppConfig::State object.
#------------------------------------------------------------------------

sub new {
    my $class = shift;
    
    my $self = {
        # internal hash arrays to store variable specification information
        VARIABLE   => { },     # variable values
        DEFAULT    => { },     # default values
        ALIAS      => { },     # known aliases  ALIAS => VARIABLE
        ALIASES    => { },     # reverse alias lookup VARIABLE => ALIASES
        ARGCOUNT   => { },     # arguments expected
        ARGS       => { },     # specific argument pattern (AppConfig::Getopt)
        EXPAND     => { },     # variable expansion (AppConfig::File)
        VALIDATE   => { },     # validation regexen or functions
        ACTION     => { },     # callback functions for when variable is set
        GLOBAL     => { },     # default global settings for new variables
        
        # other internal data
        CREATE     => 0,       # auto-create variables when set
        CASE       => 0,       # case sensitivity flag (1 = sensitive)
        PEDANTIC   => 0,       # return immediately on parse warnings
        EHANDLER   => undef,   # error handler (let's hope we don't need it!)
        ERROR      => '',      # error message
    };

    bless $self, $class;
        
    # configure if first param is a config hash ref
    $self->_configure(shift)
        if ref($_[0]) eq 'HASH';

    # call define(@_) to handle any variables definitions
    $self->define(@_)
        if @_;

    return $self;
}


#------------------------------------------------------------------------
# define($variable, \%cfg, [$variable, \%cfg, ...])
#
# Defines one or more variables.  The first parameter specifies the 
# variable name.  The following parameter may reference a hash of 
# configuration options for the variable.  Further variables and 
# configuration hashes may follow and are processed in turn.  If the 
# parameter immediately following a variable name isn't a hash reference 
# then it is ignored and the variable is defined without a specific 
# configuration, although any default parameters as specified in the 
# GLOBAL option will apply.
#
# The $variable value may contain an alias/args definition in compact
# format, such as "Foo|Bar=1".  
#
# A warning is issued (via _error()) if an invalid option is specified.
#------------------------------------------------------------------------

sub define {
    my $self = shift;
    my ($var, $args, $count, $opt, $val, $cfg, @names);

    while (@_) {
        $var = shift;
        $cfg = ref($_[0]) eq 'HASH' ? shift : { };

        # variable may be specified in compact format, 'foo|bar=i@'
        if ($var =~ s/(.+?)([!+=:].*)/$1/) {

            # anything coming after the name|alias list is the ARGS
            $cfg->{ ARGS } = $2
                if length $2;
        }

        # examine any ARGS option
        if (defined ($args = $cfg->{ ARGS })) {
          ARGGCOUNT: {
              $count = ARGCOUNT_NONE, last if $args =~ /^!/;
              $count = ARGCOUNT_LIST, last if $args =~ /@/;
              $count = ARGCOUNT_HASH, last if $args =~ /%/;
              $count = ARGCOUNT_ONE;
          }
            $cfg->{ ARGCOUNT } = $count;
        }

        # split aliases out
        @names = split(/\|/, $var);
        $var = shift @names;
        $cfg->{ ALIAS } = [ @names ] if @names;

        # variable name gets folded to lower unless CASE sensitive
        $var = lc $var unless $self->{ CASE };

        # activate $variable (so it does 'exist()') 
        $self->{ VARIABLE }->{ $var } = undef;

        # merge GLOBAL and variable-specific configurations
        $cfg = { %{ $self->{ GLOBAL } }, %$cfg };

        # examine each variable configuration parameter
        while (($opt, $val) = each %$cfg) {
            $opt = uc $opt;
            
            # DEFAULT, VALIDATE, EXPAND, ARGS and ARGCOUNT are stored as 
            # they are;
            $opt =~ /^DEFAULT|VALIDATE|EXPAND|ARGS|ARGCOUNT$/ && do {
                $self->{ $opt }->{ $var } = $val;
                next;
            };
            
            # CMDARG has been deprecated
            $opt eq 'CMDARG' && do {
                $self->_error("CMDARG has been deprecated.  "
                              . "Please use an ALIAS if required.");
                next;
            };
            
            # ACTION should be a code ref
            $opt eq 'ACTION' && do {
                unless (ref($val) eq 'CODE') {
                    $self->_error("'$opt' value is not a code reference");
                    next;
                };
                
                # store code ref, forcing keyword to upper case
                $self->{ ACTION }->{ $var } = $val;
                
                next;
            };
            
            # ALIAS creates alias links to the variable name
            $opt eq 'ALIAS' && do {
                
                # coerce $val to an array if not already so
                $val = [ split(/\|/, $val) ]
                    unless ref($val) eq 'ARRAY';
                
                # fold to lower case unless CASE sensitivity set
                unless ($self->{ CASE }) {
                    @$val = map { lc } @$val;
                }
                
                # store list of aliases...
                $self->{ ALIASES }->{ $var } = $val;
                
                # ...and create ALIAS => VARIABLE lookup hash entries
                foreach my $a (@$val) {
                    $self->{ ALIAS }->{ $a } = $var;
                }
                
                next;
            };
            
            # default 
            $self->_error("$opt is not a valid configuration item");
        }
        
        # set variable to default value
        $self->_default($var);
        
        # DEBUG: dump new variable definition
        if ($DEBUG) {
            print STDERR "Variable defined:\n";
            $self->_dump_var($var);
        }
    }
}


#------------------------------------------------------------------------
# get($variable)
#
# Returns the value of the variable specified, $variable.  Returns undef
# if the variable does not exists or is undefined and send a warning
# message to the _error() function.
#------------------------------------------------------------------------

sub get {
    my $self     = shift;
    my $variable = shift;
    my $negate   = 0;
    my $value;

    # _varname returns variable name after aliasing and case conversion
    # $negate indicates if the name got converted from "no<var>" to "<var>"
    $variable = $self->_varname($variable, \$negate);

    # check the variable has been defined
    unless (exists($self->{ VARIABLE }->{ $variable })) {
        $self->_error("$variable: no such variable");
        return undef;
    }

    # DEBUG
    print STDERR "$self->get($variable) => ", 
           defined $self->{ VARIABLE }->{ $variable }
                  ? $self->{ VARIABLE }->{ $variable }
                  : "<undef>",
          "\n"
          if $DEBUG;

    # return variable value, possibly negated if the name was "no<var>"
    $value = $self->{ VARIABLE }->{ $variable };

    return $negate ? !$value : $value;
}


#------------------------------------------------------------------------
# set($variable, $value)
#
# Assigns the value, $value, to the variable specified.
#
# Returns 1 if the variable is successfully updated or 0 if the variable 
# does not exist.  If an ACTION sub-routine exists for the variable, it 
# will be executed and its return value passed back.
#------------------------------------------------------------------------

sub set {
    my $self     = shift;
    my $variable = shift;
    my $value    = shift;
    my $negate   = 0;
    my $create;

    # _varname returns variable name after aliasing and case conversion
    # $negate indicates if the name got converted from "no<var>" to "<var>"
    $variable = $self->_varname($variable, \$negate);

    # check the variable exists
    if (exists($self->{ VARIABLE }->{ $variable })) {
        # variable found, so apply any value negation
        $value = $value ? 0 : 1 if $negate;
    }
    else {
        # auto-create variable if CREATE is 1 or a pattern matching 
        # the variable name (real name, not an alias)
        $create = $self->{ CREATE };
        if (defined $create
            && ($create eq '1' || $variable =~ /$create/)) {
            $self->define($variable);
            
            print STDERR "Auto-created $variable\n" if $DEBUG;
        }
        else {
            $self->_error("$variable: no such variable");
            return 0;
        }
    }
    
    # call the validate($variable, $value) method to perform any validation
    unless ($self->_validate($variable, $value)) {
        $self->_error("$variable: invalid value: $value");
        return 0;
    }
    
    # DEBUG
    print STDERR "$self->set($variable, ", 
    defined $value
        ? $value
        : "<undef>",
        ")\n"
        if $DEBUG;
    

    # set the variable value depending on its ARGCOUNT
    my $argcount = $self->{ ARGCOUNT }->{ $variable };
    $argcount = AppConfig::ARGCOUNT_ONE unless defined $argcount;

    if ($argcount eq AppConfig::ARGCOUNT_LIST) {
        # push value onto the end of the list
        push(@{ $self->{ VARIABLE }->{ $variable } }, $value);
    }
    elsif ($argcount eq AppConfig::ARGCOUNT_HASH) {
        # insert "<key>=<value>" data into hash 
        my ($k, $v) = split(/\s*=\s*/, $value, 2);
        # strip quoting
        $v =~ s/^(['"])(.*)\1$/$2/ if defined $v;
        $self->{ VARIABLE }->{ $variable }->{ $k } = $v;
    }
    else {
        # set simple variable
        $self->{ VARIABLE }->{ $variable } = $value;
    }


    # call any ACTION function bound to this variable
    return &{ $self->{ ACTION }->{ $variable } }($self, $variable, $value)
        if (exists($self->{ ACTION }->{ $variable }));

    # ...or just return 1 (ok)
    return 1;
}


#------------------------------------------------------------------------
# varlist($criteria, $filter)
#
# Returns a hash array of all variables and values whose real names 
# match the $criteria regex pattern passed as the first parameter.
# If $filter is set to any true value, the keys of the hash array 
# (variable names) will have the $criteria part removed.  This allows 
# the caller to specify the variables from one particular [block] and
# have the "block_" prefix removed, for example.  
#
# TODO: This should be changed to varhash().  varlist() should return a 
# list.  Also need to consider specification by list rather than regex.
#
#------------------------------------------------------------------------

sub varlist {
    my $self     = shift;
    my $criteria = shift;
    my $strip    = shift;

    $criteria = "" unless defined $criteria;

    # extract relevant keys and slice out corresponding values
    my @keys = grep(/$criteria/, keys %{ $self->{ VARIABLE } });
    my @vals = @{ $self->{ VARIABLE } }{ @keys };
    my %set;

    # clean off the $criteria part if $strip is set
    @keys = map { s/$criteria//; $_ } @keys if $strip;

    # slice values into the target hash 
    @set{ @keys } = @vals;
    return %set;
}

    
#------------------------------------------------------------------------
# AUTOLOAD
#
# Autoload function called whenever an unresolved object method is 
# called.  If the method name relates to a defined VARIABLE, we patch
# in $self->get() and $self->set() to magically update the varaiable
# (if a parameter is supplied) and return the previous value.
#
# Thus the function can be used in the folowing ways:
#    $state->variable(123);     # set a new value
#    $foo = $state->variable(); # get the current value
#
# Returns the current value of the variable, taken before any new value
# is set.  Prints a warning if the variable isn't defined (i.e. doesn't
# exist rather than exists with an undef value) and returns undef.
#------------------------------------------------------------------------

sub AUTOLOAD {
    my $self = shift;
    my ($variable, $attrib);


    # splat the leading package name
    ($variable = $AUTOLOAD) =~ s/.*:://;

    # ignore destructor
    $variable eq 'DESTROY' && return;


    # per-variable attributes and internal flags listed as keys in 
    # %METHFLAGS and %METHVARS respectively can be accessed by a 
    # method matching the attribute or flag name in lower case with 
    # a leading underscore_
    if (($attrib = $variable) =~ s/_//g) {
        $attrib = uc $attrib;
        
        if (exists $METHFLAGS{ $attrib }) {
            return $self->{ $attrib };
        }

        if (exists $METHVARS{ $attrib }) {
            # next parameter should be variable name
            $variable = shift;
            $variable = $self->_varname($variable);
            
            # check we've got a valid variable
#           $self->_error("$variable: no such variable or method"), 
#                   return undef
#               unless exists($self->{ VARIABLE }->{ $variable });
            
            # return attribute
            return $self->{ $attrib }->{ $variable };
        }
    }
    
    # set a new value if a parameter was supplied or return the old one
    return defined($_[0])
           ? $self->set($variable, shift)
           : $self->get($variable);
}



#========================================================================
#                      -----  PRIVATE METHODS -----
#========================================================================

#------------------------------------------------------------------------
# _configure(\%cfg)
#
# Sets the various configuration options using the values passed in the
# hash array referenced by $cfg.
#------------------------------------------------------------------------

sub _configure {
    my $self = shift;
    my $cfg  = shift || return;

    # construct a regex to match values which are ok to be found in GLOBAL
    my $global_ok = join('|', @GLOBAL_OK);

    foreach my $opt (keys %$cfg) {

        # GLOBAL must be a hash ref
        $opt =~ /^GLOBALS?$/i && do {
            unless (ref($cfg->{ $opt }) eq 'HASH') {
                $self->_error("\U$opt\E parameter is not a hash ref");
                next;
            }

            # we check each option is ok to be in GLOBAL, but we don't do 
            # any error checking on the values they contain (but should?).
            foreach my $global ( keys %{ $cfg->{ $opt } } )  {

                # continue if the attribute is ok to be GLOBAL 
                next if ($global =~ /(^$global_ok$)/io);
                         
                $self->_error( "\U$global\E parameter cannot be GLOBAL");
            }
            $self->{ GLOBAL } = $cfg->{ $opt };
            next;
        };
            
        # CASE, CREATE and PEDANTIC are stored as they are
        $opt =~ /^CASE|CREATE|PEDANTIC$/i && do {
            $self->{ uc $opt } = $cfg->{ $opt };
            next;
        };

        # ERROR triggers $self->_ehandler()
        $opt =~ /^ERROR$/i && do {
            $self->_ehandler($cfg->{ $opt });
            next;
        };

        # DEBUG triggers $self->_debug()
        $opt =~ /^DEBUG$/i && do {
            $self->_debug($cfg->{ $opt });
            next;
        };
            
        # warn about invalid options
        $self->_error("\U$opt\E is not a valid configuration option");
    }
}


#------------------------------------------------------------------------
# _varname($variable, \$negated)
#
# Variable names are treated case-sensitively or insensitively, depending 
# on the value of $self->{ CASE }.  When case-insensitive ($self->{ CASE } 
# != 0), all variable names are converted to lower case.  Variable values 
# are not converted.  This function simply converts the parameter 
# (variable) to lower case if $self->{ CASE } isn't set.  _varname() also 
# expands a variable alias to the name of the target variable.  
#
# Variables with an ARGCOUNT of ARGCOUNT_ZERO may be specified as 
# "no<var>" in which case, the intended value should be negated.  The 
# leading "no" part is stripped from the variable name.  A reference to 
# a scalar value can be passed as the second parameter and if the 
# _varname() method identified such a variable, it will negate the value.  
# This allows the intended value or a simple negate flag to be passed by
# reference and be updated to indicate any negation activity taking place.
#
# The (possibly modified) variable name is returned.
#------------------------------------------------------------------------

sub _varname {
    my $self     = shift;
    my $variable = shift;
    my $negated  = shift;

    # convert to lower case if case insensitive
    $variable = $self->{ CASE } ? $variable : lc $variable;

    # get the actual name if this is an alias
    $variable = $self->{ ALIAS }->{ $variable }
        if (exists($self->{ ALIAS }->{ $variable }));

    # if the variable doesn't exist, we can try to chop off a leading 
    # "no" and see if the remainder matches an ARGCOUNT_ZERO variable
    unless (exists($self->{ VARIABLE }->{ $variable })) {
        # see if the variable is specified as "no<var>"
        if ($variable =~ /^no(.*)/) {
            # see if the real variable (minus "no") exists and it
            # has an ARGOUNT of ARGCOUNT_NONE (or no ARGCOUNT at all)
            my $novar = $self->_varname($1);
            if (exists($self->{ VARIABLE }->{ $novar })
                && ! $self->{ ARGCOUNT }->{ $novar }) {
                # set variable name and negate value 
                $variable = $novar;
                $$negated = ! $$negated if defined $negated;
            }
        }
    }
    
    # return the variable name
    $variable;
}


#------------------------------------------------------------------------
# _default($variable)
#
# Sets the variable specified to the default value or undef if it doesn't
# have a default.  The default value is returned.
#------------------------------------------------------------------------

sub _default {
    my $self     = shift;
    my $variable = shift;

    # _varname returns variable name after aliasing and case conversion
    $variable = $self->_varname($variable);

    # check the variable exists
    if (exists($self->{ VARIABLE }->{ $variable })) {
        # set variable value to the default scalar, an empty list or empty
        # hash array, depending on its ARGCOUNT value
        my $argcount = $self->{ ARGCOUNT }->{ $variable };
        $argcount = AppConfig::ARGCOUNT_ONE unless defined $argcount;
        
        if ($argcount == AppConfig::ARGCOUNT_NONE) {
            return $self->{ VARIABLE }->{ $variable } 
                 = $self->{ DEFAULT }->{ $variable } || 0;
        }
        elsif ($argcount == AppConfig::ARGCOUNT_LIST) {
            my $deflist = $self->{ DEFAULT }->{ $variable };
            return $self->{ VARIABLE }->{ $variable } = 
                [ ref $deflist eq 'ARRAY' ? @$deflist : ( ) ];
            
        }
        elsif ($argcount == AppConfig::ARGCOUNT_HASH) {
            my $defhash = $self->{ DEFAULT }->{ $variable };
            return $self->{ VARIABLE }->{ $variable } = 
            { ref $defhash eq 'HASH' ? %$defhash : () };
        }
        else {
            return $self->{ VARIABLE }->{ $variable } 
                 = $self->{ DEFAULT }->{ $variable };
        }
    }
    else {
        $self->_error("$variable: no such variable");
        return 0;
    }
}


#------------------------------------------------------------------------
# _exists($variable)
#
# Returns 1 if the variable specified exists or 0 if not.
#------------------------------------------------------------------------

sub _exists {
    my $self     = shift;
    my $variable = shift;


    # _varname returns variable name after aliasing and case conversion
    $variable = $self->_varname($variable);

    # check the variable has been defined
    return exists($self->{ VARIABLE }->{ $variable });
}


#------------------------------------------------------------------------
# _validate($variable, $value)
#
# Uses any validation rules or code defined for the variable to test if
# the specified value is acceptable.
#
# Returns 1 if the value passed validation checks, 0 if not.
#------------------------------------------------------------------------

sub _validate {
    my $self     = shift;
    my $variable = shift;
    my $value    = shift;
    my $validator;


    # _varname returns variable name after aliasing and case conversion
    $variable = $self->_varname($variable);

    # return OK unless there is a validation function
    return 1 unless defined($validator = $self->{ VALIDATE }->{ $variable });

    #
    # the validation performed is based on the validator type;
    #
    #   CODE ref: code executed, returning 1 (ok) or 0 (failed)
    #   SCALAR  : a regex which should match the value
    #

    # CODE ref
    ref($validator) eq 'CODE' && do {
        # run the validation function and return the result
        return &$validator($variable, $value);
    };

    # non-ref (i.e. scalar)
    ref($validator) || do {
        # not a ref - assume it's a regex
        return $value =~ /$validator/;
    };
    
    # validation failed
    return 0;
}


#------------------------------------------------------------------------
# _error($format, @params)
#
# Checks for the existence of a user defined error handling routine and
# if defined, passes all variable straight through to that.  The routine
# is expected to handle a string format and optional parameters as per
# printf(3C).  If no error handler is defined, the message is formatted
# and passed to warn() which prints it to STDERR.
#------------------------------------------------------------------------

sub _error {
    my $self   = shift;
    my $format = shift;

    # user defined error handler?
    if (ref($self->{ EHANDLER }) eq 'CODE') {
        &{ $self->{ EHANDLER } }($format, @_);
    }
    else {
        warn(sprintf("$format\n", @_));
    }
}


#------------------------------------------------------------------------
# _ehandler($handler)
#
# Allows a new error handler to be installed.  The current value of 
# the error handler is returned.
#
# This is something of a kludge to allow other AppConfig::* modules to 
# install their own error handlers to format error messages appropriately.
# For example, AppConfig::File appends a message of the form 
# "at $file line $line" to each error message generated while parsing 
# configuration files.  The previous handler is returned (and presumably
# stored by the caller) to allow new error handlers to chain control back
# to any user-defined handler, and also restore the original handler when 
# done.
#------------------------------------------------------------------------

sub _ehandler {
    my $self    = shift;
    my $handler = shift;

    # save previous value
    my $previous = $self->{ EHANDLER };

    # update internal reference if a new handler vas provide
    if (defined $handler) {
        # check this is a code reference
        if (ref($handler) eq 'CODE') {
            $self->{ EHANDLER } = $handler;
            
            # DEBUG
            print STDERR "installed new ERROR handler: $handler\n" if $DEBUG;
        }
        else {
            $self->_error("ERROR handler parameter is not a code ref");
        }
    }
   
    return $previous;
}


#------------------------------------------------------------------------
# _debug($debug)
#
# Sets the package debugging variable, $AppConfig::State::DEBUG depending 
# on the value of the $debug parameter.  1 turns debugging on, 0 turns 
# debugging off.
#
# May be called as an object method, $state->_debug(1), or as a package
# function, AppConfig::State::_debug(1).  Returns the previous value of 
# $DEBUG, before any new value was applied.
#------------------------------------------------------------------------

sub _debug {
    # object reference may not be present if called as a package function
    my $self   = shift if ref($_[0]);
    my $newval = shift;

    # save previous value
    my $oldval = $DEBUG;

    # update $DEBUG if a new value was provided
    $DEBUG = $newval if defined $newval;

    # return previous value
    $oldval;
}


#------------------------------------------------------------------------
# _dump_var($var)
#
# Displays the content of the specified variable, $var.
#------------------------------------------------------------------------

sub _dump_var {
    my $self   = shift;
    my $var    = shift;

    return unless defined $var;

    # $var may be an alias, so we resolve the real variable name
    my $real = $self->_varname($var);
    if ($var eq $real) {
        print STDERR "$var\n";
    }
    else {
        print STDERR "$real  ('$var' is an alias)\n";
        $var = $real;
    }

    # for some bizarre reason, the variable VALUE is stored in VARIABLE
    # (it made sense at some point in time)
    printf STDERR "    VALUE        => %s\n", 
                defined($self->{ VARIABLE }->{ $var }) 
                    ? $self->{ VARIABLE }->{ $var } 
                    : "<undef>";

    # the rest of the values can be read straight out of their hashes
    foreach my $param (qw( DEFAULT ARGCOUNT VALIDATE ACTION EXPAND )) {
        printf STDERR "    %-12s => %s\n", $param, 
                defined($self->{ $param }->{ $var }) 
                    ? $self->{ $param }->{ $var } 
                    : "<undef>";
    }

    # summarise all known aliases for this variable
    print STDERR "    ALIASES      => ", 
            join(", ", @{ $self->{ ALIASES }->{ $var } }), "\n"
            if defined $self->{ ALIASES }->{ $var };
} 


#------------------------------------------------------------------------
# _dump()
#
# Dumps the contents of the Config object and all stored variables.  
#------------------------------------------------------------------------

sub _dump {
    my $self = shift;
    my $var;

    print STDERR "=" x 71, "\n";
    print STDERR 
        "Status of AppConfig::State (version $VERSION) object:\n\t$self\n";

    
    print STDERR "- " x 36, "\nINTERNAL STATE:\n";
    foreach (qw( CREATE CASE PEDANTIC EHANDLER ERROR )) {
        printf STDERR "    %-12s => %s\n", $_, 
                defined($self->{ $_ }) ? $self->{ $_ } : "<undef>";
    }       

    print STDERR "- " x 36, "\nVARIABLES:\n";
    foreach $var (keys %{ $self->{ VARIABLE } }) {
        $self->_dump_var($var);
    }

    print STDERR "- " x 36, "\n", "ALIASES:\n";
    foreach $var (keys %{ $self->{ ALIAS } }) {
        printf("    %-12s => %s\n", $var, $self->{ ALIAS }->{ $var });
    }
    print STDERR "=" x 72, "\n";
} 



1;

__END__

=head1 NAME

AppConfig::State - application configuration state

=head1 SYNOPSIS

    use AppConfig::State;

    my $state = AppConfig::State->new(\%cfg);

    $state->define("foo");            # very simple variable definition
    $state->define("bar", \%varcfg);  # variable specific configuration
    $state->define("foo|bar=i@");     # compact format

    $state->set("foo", 123);          # trivial set/get examples
    $state->get("foo");      
    
    $state->foo();                    # shortcut variable access 
    $state->foo(456);                 # shortcut variable update 

=head1 OVERVIEW

AppConfig::State is a Perl5 module to handle global configuration variables
for perl programs.  It maintains the state of any number of variables,
handling default values, aliasing, validation, update callbacks and 
option arguments for use by other AppConfig::* modules.  

AppConfig::State is distributed as part of the AppConfig bundle.

=head1 DESCRIPTION

=head2 USING THE AppConfig::State MODULE

To import and use the AppConfig::State module the following line should 
appear in your Perl script:

     use AppConfig::State;

The AppConfig::State module is loaded automatically by the new()
constructor of the AppConfig module.
      
AppConfig::State is implemented using object-oriented methods.  A 
new AppConfig::State object is created and initialised using the 
new() method.  This returns a reference to a new AppConfig::State 
object.
       
    my $state = AppConfig::State->new();

This will create a reference to a new AppConfig::State with all 
configuration options set to their default values.  You can initialise 
the object by passing a reference to a hash array containing 
configuration options:

    $state = AppConfig::State->new( {
        CASE      => 1,
        ERROR     => \&my_error,
    } );

The new() constructor of the AppConfig module automatically passes all 
parameters to the AppConfig::State new() constructor.  Thus, any global 
configuration values and variable definitions for AppConfig::State are 
also applicable to AppConfig.

The following configuration options may be specified.  

=over 4

=item CASE

Determines if the variable names are treated case sensitively.  Any non-zero
value makes case significant when naming variables.  By default, CASE is set
to 0 and thus "Variable", "VARIABLE" and "VaRiAbLe" are all treated as 
"variable".

=item CREATE

By default, CREATE is turned off meaning that all variables accessed via
set() (which includes access via shortcut such as 
C<$state-E<gt>variable($value)> which delegates to set()) must previously 
have been defined via define().  When CREATE is set to 1, calling 
set($variable, $value) on a variable that doesn't exist will cause it 
to be created automatically.

When CREATE is set to any other non-zero value, it is assumed to be a
regular expression pattern.  If the variable name matches the regex, the
variable is created.  This can be used to specify configuration file 
blocks in which variables should be created, for example:

    $state = AppConfig::State->new( {
        CREATE => '^define_',
    } );

In a config file:

    [define]
    name = fred           # define_name gets created automatically
    
    [other]
    name = john           # other_name doesn't - warning raised

Note that a regex pattern specified in CREATE is applied to the real 
variable name rather than any alias by which the variables may be 
accessed.  

=item PEDANTIC

The PEDANTIC option determines what action the configuration file 
(AppConfig::File) or argument parser (AppConfig::Args) should take 
on encountering a warning condition (typically caused when trying to set an
undeclared variable).  If PEDANTIC is set to any true value, the parsing
methods will immediately return a value of 0 on encountering such a
condition.  If PEDANTIC is not set, the method will continue to parse the
remainder of the current file(s) or arguments, returning 0 when complete.

If no warnings or errors are encountered, the method returns 1.

In the case of a system error (e.g. unable to open a file), the method
returns undef immediately, regardless of the PEDANTIC option.

=item ERROR

Specifies a user-defined error handling routine.  When the handler is 
called, a format string is passed as the first parameter, followed by 
any additional values, as per printf(3C).

=item DEBUG

Turns debugging on or off when set to 1 or 0 accordingly.  Debugging may 
also be activated by calling _debug() as an object method 
(C<$state-E<gt>_debug(1)>) or as a package function 
(C<AppConfig::State::_debug(1)>), passing in a true/false value to 
set the debugging state accordingly.  The package variable 
$AppConfig::State::DEBUG can also be set directly.  

The _debug() method returns the current debug value.  If a new value 
is passed in, the internal value is updated, but the previous value is 
returned.

Note that any AppConfig::File or App::Config::Args objects that are 
instantiated with a reference to an App::State will inherit the 
DEBUG (and also PEDANTIC) values of the state at that time.  Subsequent
changes to the AppConfig::State debug value will not affect them.

=item GLOBAL 

The GLOBAL option allows default values to be set for the DEFAULT, ARGCOUNT, 
EXPAND, VALIDATE and ACTION options for any subsequently defined variables.

    $state = AppConfig::State->new({
        GLOBAL => {
            DEFAULT  => '<undef>',     # default value for new vars
            ARGCOUNT => 1,             # vars expect an argument
            ACTION   => \&my_set_var,  # callback when vars get set
        }
    });

Any attributes specified explicitly when a variable is defined will
override any GLOBAL values.

See L<DEFINING VARIABLES> below which describes these options in detail.

=back

=head2 DEFINING VARIABLES

The C<define()> function is used to pre-declare a variable and specify 
its configuration.

    $state->define("foo");

In the simple example above, a new variable called "foo" is defined.  A 
reference to a hash array may also be passed to specify configuration 
information for the variable:

    $state->define("foo", {
            DEFAULT   => 99,
            ALIAS     => 'metavar1',
        });

Any variable-wide GLOBAL values passed to the new() constructor in the 
configuration hash will also be applied.  Values explicitly specified 
in a variable's define() configuration will override the respective GLOBAL 
values.

The following configuration options may be specified

=over 4

=item DEFAULT

The DEFAULT value is used to initialise the variable.  

    $state->define("drink", {
            DEFAULT => 'coffee',
        });

    print $state->drink();        # prints "coffee"

=item ALIAS

The ALIAS option allows a number of alternative names to be specified for 
this variable.  A single alias should be specified as a string.  Multiple 
aliases can be specified as a reference to an array of alternatives or as 
a string of names separated by vertical bars, '|'.  e.g.:

    # either
    $state->define("name", {
            ALIAS  => 'person',
        });

    # or
    $state->define("name", {
            ALIAS => [ 'person', 'user', 'uid' ],
        });
    
    # or
    $state->define("name", {
            ALIAS => 'person|user|uid',
        });
    
    $state->user('abw');     # equivalent to $state->name('abw');

=item ARGCOUNT

The ARGCOUNT option specifies the number of arguments that should be 
supplied for this variable.  By default, no additional arguments are 
expected for variables (ARGCOUNT_NONE).

The ARGCOUNT_* constants can be imported from the AppConfig module:

    use AppConfig ':argcount';

    $state->define('foo', { ARGCOUNT => ARGCOUNT_ONE });

or can be accessed directly from the AppConfig package:

    use AppConfig;

    $state->define('foo', { ARGCOUNT => AppConfig::ARGCOUNT_ONE });

The following values for ARGCOUNT may be specified.  

=over 4

=item ARGCOUNT_NONE (0)

Indicates that no additional arguments are expected.  If the variable is
identified in a confirguration file or in the command line arguments, it
is set to a value of 1 regardless of whatever arguments follow it.

=item ARGCOUNT_ONE (1)

Indicates that the variable expects a single argument to be provided.
The variable value will be overwritten with a new value each time it 
is encountered.

=item ARGCOUNT_LIST (2)

Indicates that the variable expects multiple arguments.  The variable 
value will be appended to the list of previous values each time it is
encountered.  

=item ARGCOUNT_HASH (3)

Indicates that the variable expects multiple arguments and that each
argument is of the form "key=value".  The argument will be split into 
a key/value pair and inserted into the hash of values each time it 
is encountered.

=back

=item ARGS

The ARGS option can also be used to specify advanced command line options 
for use with AppConfig::Getopt, which itself delegates to Getopt::Long.  
See those two modules for more information on the format and meaning of
these options.

    $state->define("name", {
            ARGS => "=i@",
        });

=item EXPAND 

The EXPAND option specifies how the AppConfig::File processor should 
expand embedded variables in the configuration file values it reads.
By default, EXPAND is turned off (EXPAND_NONE) and no expansion is made.  

The EXPAND_* constants can be imported from the AppConfig module:

    use AppConfig ':expand';

    $state->define('foo', { EXPAND => EXPAND_VAR });

or can be accessed directly from the AppConfig package:

    use AppConfig;

    $state->define('foo', { EXPAND => AppConfig::EXPAND_VAR });

The following values for EXPAND may be specified.  Multiple values should
be combined with vertical bars , '|', e.g. C<EXPAND_UID | EXPAND_VAR>).

=over 4

=item EXPAND_NONE

Indicates that no variable expansion should be attempted.

=item EXPAND_VAR

Indicates that variables embedded as $var or $(var) should be expanded
to the values of the relevant AppConfig::State variables.

=item EXPAND_UID 

Indicates that '~' or '~uid' patterns in the string should be 
expanded to the current users ($<), or specified user's home directory.
In the first case, C<~> is expanded to the value of the C<HOME>
environment variable.  In the second case, the C<getpwnam()> method
is used if it is available on your system (which it isn't on Win32).

=item EXPAND_ENV

Inidicates that variables embedded as ${var} should be expanded to the 
value of the relevant environment variable.

=item EXPAND_ALL

Equivalent to C<EXPAND_VARS | EXPAND_UIDS | EXPAND_ENVS>).

=item EXPAND_WARN

Indicates that embedded variables that are not defined should raise a
warning.  If PEDANTIC is set, this will cause the read() method to return 0
immediately.

=back

=item VALIDATE

Each variable may have a sub-routine or regular expression defined which 
is used to validate the intended value for a variable before it is set.

If VALIDATE is defined as a regular expression, it is applied to the
value and deemed valid if the pattern matches.  In this case, the
variable is then set to the new value.  A warning message is generated
if the pattern match fails.

VALIDATE may also be defined as a reference to a sub-routine which takes
as its arguments the name of the variable and its intended value.  The 
sub-routine should return 1 or 0 to indicate that the value is valid
or invalid, respectively.  An invalid value will cause a warning error
message to be generated.

If the GLOBAL VALIDATE variable is set (see GLOBAL in L<DESCRIPTION> 
above) then this value will be used as the default VALIDATE for each 
variable unless otherwise specified.

    $state->define("age", {
            VALIDATE => '\d+',
        });

    $state->define("pin", {
            VALIDATE => \&check_pin,
        });

=item ACTION

The ACTION option allows a sub-routine to be bound to a variable as a
callback that is executed whenever the variable is set.  The ACTION is
passed a reference to the AppConfig::State object, the name of the
variable and the value of the variable.

The ACTION routine may be used, for example, to post-process variable
data, update the value of some other dependant variable, generate a
warning message, etc.

Example:

    $state->define("foo", { ACTION => \&my_notify });

    sub my_notify {
        my $state = shift;
        my $var   = shift;
        my $val   = shift;

        print "$variable set to $value";
    }

    $state->foo(42);        # prints "foo set to 42"

Be aware that calling C<$state-E<gt>set()> to update the same variable
from within the ACTION function will cause a recursive loop as the
ACTION function is repeatedly called.  

=item 

=back

=head2 DEFINING VARIABLES USING THE COMPACT FORMAT

Variables may be defined in a compact format which allows any ALIAS and
ARGS values to be specified as part of the variable name.  This is designed
to mimic the behaviour of Johan Vromans' Getopt::Long module.

Aliases for a variable should be specified after the variable name, 
separated by vertical bars, '|'.  Any ARGS parameter should be appended 
after the variable name(s) and/or aliases.

The following examples are equivalent:

    $state->define("foo", { 
            ALIAS => [ 'bar', 'baz' ],
            ARGS  => '=i',
        });

    $state->define("foo|bar|baz=i");

=head2 READING AND MODIFYING VARIABLE VALUES

AppConfig::State defines two methods to manipulate variable values: 

    set($variable, $value);
    get($variable);

Both functions take the variable name as the first parameter and
C<set()> takes an additional parameter which is the new value for the
variable.  C<set()> returns 1 or 0 to indicate successful or
unsuccessful update of the variable value.  If there is an ACTION
routine associated with the named variable, the value returned will be
passed back from C<set()>.  The C<get()> function returns the current
value of the variable.

Once defined, variables may be accessed directly as object methods where
the method name is the same as the variable name.  i.e.

    $state->set("verbose", 1);

is equivalent to 

    $state->verbose(1); 

Without parameters, the current value of the variable is returned.  If
a parameter is specified, the variable is set to that value and the 
result of the set() operation is returned.

    $state->age(29);        # sets 'age' to 29, returns 1 (ok)

=head2 INTERNAL METHODS

The interal (private) methods of the AppConfig::State class are listed 
below.

They aren't intended for regular use and potential users should consider
the fact that nothing about the internal implementation is guaranteed to
remain the same.  Having said that, the AppConfig::State class is
intended to co-exist and work with a number of other modules and these
are considered "friend" classes.  These methods are provided, in part,
as services to them.  With this acknowledged co-operation in mind, it is
safe to assume some stability in this core interface.

The _varname() method can be used to determine the real name of a variable 
from an alias:

    $varname->_varname($alias);

Note that all methods that take a variable name, including those listed
below, can accept an alias and automatically resolve it to the correct 
variable name.  There is no need to call _varname() explicitly to do 
alias expansion.  The _varname() method will fold all variables names
to lower case unless CASE sensititvity is set.

The _exists() method can be used to check if a variable has been
defined:

    $state->_exists($varname);

The _default() method can be used to reset a variable to its default value:

    $state->_default($varname);

The _expand() method can be used to determine the EXPAND value for a 
variable:

    print "$varname EXPAND: ", $state->_expand($varname), "\n";

The _argcount() method returns the value of the ARGCOUNT attribute for a 
variable:

    print "$varname ARGCOUNT: ", $state->_argcount($varname), "\n";

The _validate() method can be used to determine if a new value for a variable
meets any validation criteria specified for it.  The variable name and 
intended value should be passed in.  The methods returns a true/false value
depending on whether or not the validation succeeded:

    print "OK\n" if $state->_validate($varname, $value);

The _pedantic() method can be called to determine the current value of the
PEDANTIC option.

    print "pedantic mode is ", $state->_pedantic() ? "on" ; "off", "\n";

The _debug() method can be used to turn debugging on or off (pass 1 or 0
as a parameter).  It can also be used to check the debug state,
returning the current internal value of $AppConfig::State::DEBUG.  If a
new debug value is provided, the debug state is updated and the previous
state is returned.

    $state->_debug(1);               # debug on, returns previous value

The _dump_var($varname) and _dump() methods may also be called for
debugging purposes.  

    $state->_dump_var($varname);    # show variable state
    $state->_dump();                # show internal state and all vars

=head1 AUTHOR

Andy Wardley, E<lt>abw@wardley.orgE<gt>

=head1 COPYRIGHT

Copyright (C) 1997-2007 Andy Wardley.  All Rights Reserved.

Copyright (C) 1997,1998 Canon Research Centre Europe Ltd.

This module is free software; you can redistribute it and/or modify it 
under the same terms as Perl itself.

=head1 SEE ALSO

AppConfig, AppConfig::File, AppConfig::Args, AppConfig::Getopt

=cut
