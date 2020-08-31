#============================================================================
#
# AppConfig::File.pm
#
# Perl5 module to read configuration files and use the contents therein 
# to update variable values in an AppConfig::State object.
#
# Written by Andy Wardley <abw@wardley.org>
#
# Copyright (C) 1997-2007 Andy Wardley.  All Rights Reserved.
# Copyright (C) 1997,1998 Canon Research Centre Europe Ltd.
#
#============================================================================

package AppConfig::File;
use 5.006;
use strict;
use warnings;
use AppConfig;
use AppConfig::State;
our $VERSION = '1.71';


#------------------------------------------------------------------------
# new($state, $file, [$file, ...])
#
# Module constructor.  The first, mandatory parameter should be a 
# reference to an AppConfig::State object to which all actions should 
# be applied.  The remaining parameters are assumed to be file names or
# file handles for reading and are passed to parse().
#
# Returns a reference to a newly created AppConfig::File object.
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

    # call parse(@_) to parse any files specified as further params
    $self->parse(@_) if @_;

    return $self;
}


#------------------------------------------------------------------------
# parse($file, [file, ...])
#
# Reads and parses a config file, updating the contents of the 
# AppConfig::State referenced by $self->{ STATE } according to the 
# contents of the file.  Multiple files may be specified and are 
# examined in turn.  The method reports any error condition via 
# $self->{ STATE }->_error() and immediately returns undef if it 
# encounters a system error (i.e. cannot open one of the files.  
# Parsing errors such as unknown variables or unvalidated values will 
# also cause warnings to be raised vi the same _error(), but parsing
# continues to the end of the current file and through any subsequent
# files.  If the PEDANTIC option is set in the $self->{ STATE } object, 
# the behaviour is overridden and the method returns 0 immediately on 
# any system or parsing error.
#
# The EXPAND option for each variable determines how the variable
# value should be expanded.
#
# Returns undef on system error, 0 if all files were parsed but generated
# one or more warnings, 1 if all files parsed without warnings.
#------------------------------------------------------------------------

sub parse {
    my $self     = shift;
    my $warnings = 0;
    my $prefix;           # [block] defines $prefix
    my $file;
    my $flag;

    # take a local copy of the state to avoid much hash dereferencing
    my ($state, $debug, $pedantic) = @$self{ qw( STATE DEBUG PEDANTIC ) };

    # we want to install a custom error handler into the AppConfig::State 
    # which appends filename and line info to error messages and then 
    # calls the previous handler;  we start by taking a copy of the 
    # current handler..
    my $errhandler = $state->_ehandler();

    # ...and if it doesn't exist, we craft a default handler
    $errhandler = sub { warn(sprintf(shift, @_), "\n") }
        unless defined $errhandler;

    # install a closure as a new error handler
    $state->_ehandler(
        sub {
            # modify the error message 
            my $format  = shift;
               $format .= ref $file 
                          ? " at line $."
                          : " at $file line $.";

            # chain call to prevous handler
            &$errhandler($format, @_);
        }
    );

    # trawl through all files passed as params
    FILE: while ($file = shift) {

        # local/lexical vars ensure opened files get closed
        my $handle;
        local *FH;

        # if the file is a reference, we assume it's a file handle, if
        # not, we assume it's a filename and attempt to open it
        $handle = $file;
        if (ref($file)) {
            $handle = $file;

            # DEBUG
            print STDERR "reading from file handle: $file\n" if $debug;
        }
        else {
            # open and read config file
            open(FH, $file) or do {
                # restore original error handler and report error
                $state->_ehandler($errhandler);
                $state->_error("$file: $!");

                return undef;
            };
            $handle = \*FH;

            # DEBUG
            print STDERR "reading file: $file\n" if $debug;
        }

        # initialise $prefix to nothing (no [block])
        $prefix = '';

        local $_;
        while (<$handle>) {
            chomp;

            # Throw away everything from an unescaped # to EOL
            s/(^|\s+)#.*/$1/;

            # add next line if there is one and this is a continuation
            if (s/\\$// && !eof($handle)) {
                $_ .= <$handle>;
                redo;
            }

            # Convert \# -> #
            s/\\#/#/g;

            # ignore blank lines
            next if /^\s*$/;

            # strip leading and trailing whitespace
            s/^\s+//;
            s/\s+$//;

            # look for a [block] to set $prefix
            if (/^\[([^\]]+)\]$/) {
                $prefix = $1;
                print STDERR "Entering [$prefix] block\n" if $debug;
                next;
            }

            # split line up by whitespace (\s+) or "equals" (\s*=\s*)
            if (/^([^\s=]+)(?:(?:(?:\s*=\s*)|\s+)(.*))?/) {
                my ($variable, $value) = ($1, $2);

                if (defined $value) {
                    # here document
                    if ($value =~ /^([^\s=]+\s*=)?\s*<<(['"]?)(\S+)\2$/) { # '<<XX' or 'hashkey =<<XX'
                        my $boundary = "$3\n";
                        $value = defined($1) ? $1 : '';
                        while (<$handle>) {
                            last if $_ eq $boundary;
                            $value .= $_;
                        };
                        $value =~ s/[\r\n]$//;
                    } else {
                        # strip any quoting from the variable value
                        $value =~ s/^(['"])(.*)\1$/$2/;
                    };
                };

                # strip any leading '+/-' from the variable
                $variable =~ s/^([\-+]?)//;
                $flag = $1;

                # $variable gets any $prefix 
                $variable = $prefix . '_' . $variable
                    if length $prefix;

                # if the variable doesn't exist, we call set() to give 
                # AppConfig::State a chance to auto-create it
                unless ($state->_exists($variable) 
                            || $state->set($variable, 1)) {
                    $warnings++;
                    last FILE if $pedantic;
                    next;
                }       

                my $nargs = $state->_argcount($variable);

                # variables prefixed '-' are reset to their default values
                if ($flag eq '-') {
                    $state->_default($variable);
                    next;
                }
                # those prefixed '+' get set to 1
                elsif ($flag eq '+') {
                    $value = 1 unless defined $value;
                }

                # determine if any extra arguments were expected
                if ($nargs) {
                    if (defined $value && length $value) {
                        # expand any embedded variables, ~uids or
                        # environment variables, testing the return value
                        # for errors;  we pass in any variable-specific
                        # EXPAND value 
                        unless ($self->_expand(\$value, 
                                $state->_expand($variable), $prefix)) {
                            print STDERR "expansion of [$value] failed\n" 
                                if $debug;
                            $warnings++;
                            last FILE if $pedantic;
                        }
                    }
                    else {
                        $state->_error("$variable expects an argument");
                        $warnings++;
                        last FILE if $pedantic;
                        next;
                    }
                }
                # $nargs = 0
                else {
                    # default value to 1 unless it is explicitly defined
                    # as '0' or "off"
                    if (defined $value) {
                        # "off" => 0
                        $value = 0 if $value =~ /off/i;
                        # any value => 1
                        $value = 1 if $value;
                    }
                    else {
                        # assume 1 unless explicitly defined off/0
                        $value = 1;
                    }
                    print STDERR "$variable => $value (no expansion)\n"
                        if $debug;
                }

                # set the variable, noting any failure from set()
                unless ($state->set($variable, $value)) {
                    $warnings++;
                    last FILE if $pedantic;
                }
            }
            else {
                $state->_error("parse error");
                $warnings++;
            }
        }
    }

    # restore original error handler
    $state->_ehandler($errhandler);

    # return $warnings => 0, $success => 1
    return $warnings ? 0 : 1;
}



#========================================================================
#                      -----  PRIVATE METHODS -----
#========================================================================

#------------------------------------------------------------------------
# _expand(\$value, $expand, $prefix)
#
# The variable value string, referenced by $value, is examined and any 
# embedded variables, environment variables or tilde globs (home 
# directories) are replaced with their respective values, depending on 
# the value of the second parameter, $expand.  The third paramter may
# specify the name of the current [block] in which the parser is 
# parsing.  This prefix is prepended to any embedded variable name that
# can't otherwise be resolved.  This allows the following to work:
#
#   [define]
#   home = /home/abw
#   html = $define_home/public_html
#   html = $home/public_html     # same as above, 'define' is prefix
#
# Modifications are made directly into the variable referenced by $value.
# The method returns 1 on success or 0 if any warnings (undefined 
# variables) were encountered.
#------------------------------------------------------------------------

sub _expand {
    my ($self, $value, $expand, $prefix) = @_;
    my $warnings = 0;
    my ($sys, $var, $val);


    # ensure prefix contains something (nothing!) valid for length()
    $prefix = "" unless defined $prefix;

    # take a local copy of the state to avoid much hash dereferencing
    my ($state, $debug, $pedantic) = @$self{ qw( STATE DEBUG PEDANTIC ) };

    # bail out if there's nothing to do
    return 1 unless $expand && defined($$value);

    # create an AppConfig::Sys instance, or re-use a previous one, 
    # to handle platform dependant functions: getpwnam(), getpwuid()
    unless ($sys = $self->{ SYS }) {
        require AppConfig::Sys;
        $sys = $self->{ SYS } = AppConfig::Sys->new();
    }

    print STDERR "Expansion of [$$value] " if $debug;

    EXPAND: {

        # 
        # EXPAND_VAR
        # expand $(var) and $var as AppConfig::State variables
        #
        if ($expand & AppConfig::EXPAND_VAR) {

            $$value =~ s{
                (?<!\\)\$ (?: \((\w+)\) | (\w+) ) # $2 => $(var) | $3 => $var

            } {
                # embedded variable name will be one of $2 or $3
                $var = defined $1 ? $1 : $2;

                # expand the variable if defined
                if ($state->_exists($var)) {
                    $val = $state->get($var);
                }
                elsif (length $prefix 
                        && $state->_exists($prefix . '_' . $var)) {
                    print STDERR "(\$$var => \$${prefix}_$var) "
                        if $debug;
                    $var = $prefix . '_' . $var;
                    $val = $state->get($var);
                }
                else {
                    # raise a warning if EXPAND_WARN set
                    if ($expand & AppConfig::EXPAND_WARN) {
                        $state->_error("$var: no such variable");
                        $warnings++;
                    }

                    # replace variable with nothing
                    $val = '';
                }

                # $val gets substituted back into the $value string
                $val;
            }gex;

            $$value =~ s/\\\$/\$/g;

            # bail out now if we need to
            last EXPAND if $warnings && $pedantic;
        }


        #
        # EXPAND_UID
        # expand ~uid as home directory (for $< if uid not specified)
        #
        if ($expand & AppConfig::EXPAND_UID) {
            $$value =~ s{
                ~(\w+)?                    # $1 => username (optional)
            } {
                $val = undef;

                # embedded user name may be in $1
                if (defined ($var = $1)) {
                    # try and get user's home directory
                    if ($sys->can_getpwnam()) {
                        $val = ($sys->getpwnam($var))[7];
                    }
                } else {
                    # determine home directory 
                    $val = $ENV{ HOME };
                }

                # catch-all for undefined $dir
                unless (defined $val) {
                    # raise a warning if EXPAND_WARN set
                    if ($expand & AppConfig::EXPAND_WARN) {
                        $state->_error("cannot determine home directory%s",
                            defined $var ? " for $var" : "");
                        $warnings++;
                    }

                    # replace variable with nothing
                    $val = '';
                }

                # $val gets substituted back into the $value string
                $val;
            }gex;

            # bail out now if we need to
            last EXPAND if $warnings && $pedantic;
        }


        #
        # EXPAND_ENV
        # expand ${VAR} as environment variables
        #
        if ($expand & AppConfig::EXPAND_ENV) {

            $$value =~ s{ 
                ( \$ \{ (\w+) \} )
            } {
                $var = $2;

                # expand the variable if defined
                if (exists $ENV{ $var }) {
                    $val = $ENV{ $var };
                } elsif ( $var eq 'HOME' ) {
                    # In the special case of HOME, if not set
                    # use the internal version
                    $val = $self->{ HOME };
                } else {
                    # raise a warning if EXPAND_WARN set
                    if ($expand & AppConfig::EXPAND_WARN) {
                        $state->_error("$var: no such environment variable");
                        $warnings++;
                    }

                    # replace variable with nothing
                    $val = '';
                }
                # $val gets substituted back into the $value string
                $val;
            }gex;

            # bail out now if we need to
            last EXPAND if $warnings && $pedantic;
        }
    }

    print STDERR "=> [$$value] (EXPAND = $expand)\n" if $debug;

    # return status 
    return $warnings ? 0 : 1;
}



#------------------------------------------------------------------------
# _dump()
#
# Dumps the contents of the Config object.
#------------------------------------------------------------------------

sub _dump {
    my $self = shift;

    foreach my $key (keys %$self) {
        printf("%-10s => %s\n", $key, 
                defined($self->{ $key }) ? $self->{ $key } : "<undef>");
    }       
} 



1;

__END__

=head1 NAME

AppConfig::File - Perl5 module for reading configuration files.

=head1 SYNOPSIS

    use AppConfig::File;

    my $state   = AppConfig::State->new(\%cfg1);
    my $cfgfile = AppConfig::File->new($state, $file);

    $cfgfile->parse($file);            # read config file

=head1 OVERVIEW

AppConfig::File is a Perl5 module which reads configuration files and use 
the contents therein to update variable values in an AppConfig::State 
object.

AppConfig::File is distributed as part of the AppConfig bundle.

=head1 DESCRIPTION

=head2 USING THE AppConfig::File MODULE

To import and use the AppConfig::File module the following line should appear
in your Perl script:

    use AppConfig::File;

AppConfig::File is used automatically if you use the AppConfig module 
and create an AppConfig::File object through the file() method.

AppConfig::File is implemented using object-oriented methods.  A new 
AppConfig::File object is created and initialised using the 
AppConfig::File->new() method.  This returns a reference to a new 
AppConfig::File object.  A reference to an AppConfig::State object 
should be passed in as the first parameter:

    my $state   = AppConfig::State->new();
    my $cfgfile = AppConfig::File->new($state);

This will create and return a reference to a new AppConfig::File object.

=head2 READING CONFIGURATION FILES 

The C<parse()> method is used to read a configuration file and have the 
contents update the STATE accordingly.

    $cfgfile->parse($file);

Multiple files maye be specified and will be read in turn.

    $cfgfile->parse($file1, $file2, $file3);

The method will return an undef value if it encounters any errors opening
the files.  It will return immediately without processing any further files.
By default, the PEDANTIC option in the AppConfig::State object, 
$self->{ STATE }, is turned off and any parsing errors (invalid variables,
unvalidated values, etc) will generated warnings, but not cause the method
to return.  Having processed all files, the method will return 1 if all
files were processed without warning or 0 if one or more warnings were
raised.  When the PEDANTIC option is turned on, the method generates a
warning and immediately returns a value of 0 as soon as it encounters any
parsing error.

Variables values in the configuration files may be expanded depending on 
the value of their EXPAND option, as determined from the App::State object.
See L<AppConfig::State> for more information on variable expansion.

=head2 CONFIGURATION FILE FORMAT

A configuration file may contain blank lines and comments which are
ignored.  Comments begin with a '#' as the first character on a line
or following one or more whitespace tokens, and continue to the end of
the line.

    # this is a comment
    foo = bar               # so is this
    url = index.html#hello  # this too, but not the '#welcome'

Notice how the '#welcome' part of the URL is not treated as a comment
because a whitespace character doesn't precede it.  

Long lines can be continued onto the next line by ending the first 
line with a '\'.

    callsign = alpha bravo camel delta echo foxtrot golf hipowls \
               india juliet kilo llama mike november oscar papa  \
               quebec romeo sierra tango umbrella victor whiskey \
               x-ray yankee zebra

Variables that are simple flags and do not expect an argument (ARGCOUNT = 
ARGCOUNT_NONE) can be specified without any value.  They will be set with 
the value 1, with any value explicitly specified (except "0" and "off")
being ignored.  The variable may also be specified with a "no" prefix to 
implicitly set the variable to 0.

    verbose                              # on  (1)
    verbose = 1                          # on  (1)
    verbose = 0                          # off (0)
    verbose off                          # off (0)
    verbose on                           # on  (1)
    verbose mumble                       # on  (1)
    noverbose                            # off (0)

Variables that expect an argument (ARGCOUNT = ARGCOUNT_ONE) will be set to 
whatever follows the variable name, up to the end of the current line.  An
equals sign may be inserted between the variable and value for clarity.

    room = /home/kitchen     
    room   /home/bedroom

Each subsequent re-definition of the variable value overwrites the previous
value.

    print $config->room();               # prints "/home/bedroom"

Variables may be defined to accept multiple values (ARGCOUNT = ARGCOUNT_LIST).
Each subsequent definition of the variable adds the value to the list of
previously set values for the variable.  

    drink = coffee
    drink = tea

A reference to a list of values is returned when the variable is requested.

    my $beverages = $config->drinks();
    print join(", ", @$beverages);      # prints "coffee, tea"

Variables may also be defined as hash lists (ARGCOUNT = ARGCOUNT_HASH).
Each subsequent definition creates a new key and value in the hash array.

    alias l="ls -CF"
    alias h="history"

A reference to the hash is returned when the variable is requested.

    my $aliases = $config->alias();
    foreach my $k (keys %$aliases) {
        print "$k => $aliases->{ $k }\n";
    }

A large chunk of text can be defined using Perl's "heredoc" quoting
style.

   scalar = <<BOUNDARY_STRING
   line 1
   line 2: Space/linebreaks within a HERE document are kept.
   line 3: The last linebreak (\n) is stripped.
   BOUNDARY_STRING

   hash   key1 = <<'FOO'
     * Quotes (['"]) around the boundary string are simply ignored.
     * Whether the variables in HERE document are expanded depends on
       the EXPAND option of the variable or global setting.
   FOO

   hash = key2 = <<"_bar_"
   Text within HERE document are kept as is.
   # comments are treated as a normal text.
   The same applies to line continuation. \
   _bar_

Note that you cannot use HERE document as a key in a hash or a name 
of a variable.

The '-' prefix can be used to reset a variable to its default value and
the '+' prefix can be used to set it to 1

    -verbose
    +debug

Variable, environment variable and tilde (home directory) expansions
Variable values may contain references to other AppConfig variables, 
environment variables and/or users' home directories.  These will be 
expanded depending on the EXPAND value for each variable or the GLOBAL
EXPAND value.

Three different expansion types may be applied:

    bin = ~/bin          # expand '~' to home dir if EXPAND_UID
    tmp = ~abw/tmp       # as above, but home dir for user 'abw'

    perl = $bin/perl     # expand value of 'bin' variable if EXPAND_VAR
    ripl = $(bin)/ripl   # as above with explicit parens

    home = ${HOME}       # expand HOME environment var if EXPAND_ENV

See L<AppConfig::State> for more information on expanding variable values.

The configuration files may have variables arranged in blocks.  A block 
header, consisting of the block name in square brackets, introduces a 
configuration block.  The block name and an underscore are then prefixed 
to the names of all variables subsequently referenced in that block.  The 
block continues until the next block definition or to the end of the current 
file.

    [block1]
    foo = 10             # block1_foo = 10

    [block2]
    foo = 20             # block2_foo = 20

=head1 AUTHOR

Andy Wardley, E<lt>abw@wardley.orgE<gt>

=head1 COPYRIGHT

Copyright (C) 1997-2007 Andy Wardley.  All Rights Reserved.

This module is free software; you can redistribute it and/or modify it 
under the same terms as Perl itself.

=head1 SEE ALSO

AppConfig, AppConfig::State

=cut
