#============================================================================
#
# AppConfig.pm
#
# Perl5 module for reading and parsing configuration files and command line 
# arguments.
#
# Written by Andy Wardley <abw@wardley.org>
#
# Copyright (C) 1997-2007 Andy Wardley.  All Rights Reserved.
# Copyright (C) 1997,1998 Canon Research Centre Europe Ltd.
#==========================================================================

package AppConfig;

use 5.006;
use strict;
use warnings;
use base 'Exporter';
our $VERSION = '1.71';

# variable expansion constants
use constant EXPAND_NONE   => 0;
use constant EXPAND_VAR    => 1;
use constant EXPAND_UID    => 2;
use constant EXPAND_ENV    => 4;
use constant EXPAND_ALL    => EXPAND_VAR | EXPAND_UID | EXPAND_ENV;
use constant EXPAND_WARN   => 8;

# argument count types
use constant ARGCOUNT_NONE => 0;
use constant ARGCOUNT_ONE  => 1;
use constant ARGCOUNT_LIST => 2;
use constant ARGCOUNT_HASH => 3;

# Exporter tagsets
our @EXPAND = qw(
    EXPAND_NONE
    EXPAND_VAR
    EXPAND_UID
    EXPAND_ENV 
    EXPAND_ALL
    EXPAND_WARN
);

our @ARGCOUNT = qw(
    ARGCOUNT_NONE
    ARGCOUNT_ONE
    ARGCOUNT_LIST
    ARGCOUNT_HASH
);

our @EXPORT_OK   = ( @EXPAND, @ARGCOUNT );
our %EXPORT_TAGS = (
    expand   => [ @EXPAND   ],
    argcount => [ @ARGCOUNT ],
);
our $AUTOLOAD;

require AppConfig::State;

#------------------------------------------------------------------------
# new(\%config, @vars)
#
# Module constructor.  All parameters passed are forwarded onto the 
# AppConfig::State constructor.  Returns a reference to a newly created 
# AppConfig object.
#------------------------------------------------------------------------

sub new {
    my $class = shift;
    bless {
        STATE => AppConfig::State->new(@_)
    }, $class;
}


#------------------------------------------------------------------------
# file(@files)
#
# The file() method is called to parse configuration files.  An 
# AppConfig::File object is instantiated and stored internally for
# use in subsequent calls to file().
#------------------------------------------------------------------------

sub file {
    my $self  = shift;
    my $state = $self->{ STATE };
    my $file;

    require AppConfig::File;

    # create an AppConfig::File object if one isn't defined 
    $file = $self->{ FILE } ||= AppConfig::File->new($state);

    # call on the AppConfig::File object to process files.
    $file->parse(@_);
}


#------------------------------------------------------------------------
# args(\@args)
#
# The args() method is called to parse command line arguments.  An 
# AppConfig::Args object is instantiated and then stored internally for
# use in subsequent calls to args().
#------------------------------------------------------------------------

sub args {
    my $self  = shift;
    my $state = $self->{ STATE };
    my $args;

    require AppConfig::Args;

    # create an AppConfig::Args object if one isn't defined
    $args = $self->{ ARGS } ||= AppConfig::Args->new($state);

    # call on the AppConfig::Args object to process arguments.
    $args->parse(shift);
}


#------------------------------------------------------------------------
# getopt(@config, \@args)
#
# The getopt() method is called to parse command line arguments.  The
# AppConfig::Getopt module is require()'d and an AppConfig::Getopt object
# is created to parse the arguments.
#------------------------------------------------------------------------

sub getopt {
    my $self  = shift;
    my $state = $self->{ STATE };
    my $getopt;

    require AppConfig::Getopt;

    # create an AppConfig::Getopt object if one isn't defined
    $getopt = $self->{ GETOPT } ||= AppConfig::Getopt->new($state);

    # call on the AppConfig::Getopt object to process arguments.
    $getopt->parse(@_);
}


#------------------------------------------------------------------------
# cgi($query)
#
# The cgi() method is called to parse a CGI query string.  An 
# AppConfig::CGI object is instantiated and then stored internally for
# use in subsequent calls to args().
#------------------------------------------------------------------------

sub cgi {
    my $self  = shift;
    my $state = $self->{ STATE };
    my $cgi;

    require AppConfig::CGI;

    # create an AppConfig::CGI object if one isn't defined
    $cgi = $self->{ CGI } ||= AppConfig::CGI->new($state);

    # call on the AppConfig::CGI object to process a query.
    $cgi->parse(shift);
}

#------------------------------------------------------------------------
# AUTOLOAD
#
# Autoload function called whenever an unresolved object method is 
# called.  All methods are delegated to the $self->{ STATE } 
# AppConfig::State object.
#
#------------------------------------------------------------------------

sub AUTOLOAD {
    my $self = shift;
    my $method;

    # splat the leading package name
    ($method = $AUTOLOAD) =~ s/.*:://;

    # ignore destructor
    $method eq 'DESTROY' && return;

    # delegate method call to AppConfig::State object in $self->{ STATE } 
    $self->{ STATE }->$method(@_);
}

1;

__END__

=head1 NAME

AppConfig - Perl5 module for reading configuration files and parsing command line arguments.

=head1 SYNOPSIS

    use AppConfig;

    # create a new AppConfig object
    my $config = AppConfig->new( \%cfg );

    # define a new variable
    $config->define( $varname => \%varopts );

    # create/define combined
    my $config = AppConfig->new( \%cfg, 
        $varname => \%varopts,
        $varname => \%varopts,
        ...
    );

    # set/get the value
    $config->set( $varname, $value );
    $config->get($varname);

    # shortcut form
    $config->varname($value);
    $config->varname;

    # read configuration file
    $config->file($file);

    # parse command line options
    $config->args(\@args);      # default to \@ARGV

    # advanced command line options with Getopt::Long
    $config->getopt(\@args);    # default to \@ARGV

    # parse CGI parameters (GET method)
    $config->cgi($query);       # default to $ENV{ QUERY_STRING }

=head1 OVERVIEW

AppConfig is a Perl5 module for managing application configuration 
information.  It maintains the state of any number of variables and 
provides methods for parsing configuration files, command line 
arguments and CGI script parameters.

Variables values may be set via configuration files.  Variables may be 
flags (On/Off), take a single value, or take multiple values stored as a
list or hash.  The number of arguments a variable expects is determined
by its configuration when defined.

    # flags
    verbose 
    nohelp
    debug = On

    # single value
    home  = /home/abw/

    # multiple list value
    file = /tmp/file1
    file = /tmp/file2

    # multiple hash value
    book  camel = Programming Perl
    book  llama = Learning Perl

The '-' prefix can be used to reset a variable to its default value and
the '+' prefix can be used to set it to 1

    -verbose
    +debug

Variable, environment variable and tilde (home directory) expansions
can be applied (selectively, if necessary) to the values read from 
configuration files:

    home = ~                    # home directory
    nntp = ${NNTPSERVER}        # environment variable
    html = $home/html           # internal variables
    img  = $html/images

Configuration files may be arranged in blocks as per the style of Win32 
"INI" files.

    [file]
    site = kfs
    src  = ~/websrc/docs/$site
    lib  = ~/websrc/lib
    dest = ~/public_html/$site

    [page]
    header = $lib/header
    footer = $lib/footer

You can also use Perl's "heredoc" syntax to define a large block of
text in a configuration file.

    multiline = <<FOOBAR
    line 1
    line 2
    FOOBAR

    paths  exe  = "${PATH}:${HOME}/.bin"
    paths  link = <<'FOO'
    ${LD_LIBARRAY_PATH}:${HOME}/lib
    FOO

Variables may also be set by parsing command line arguments.

    myapp -verbose -site kfs -file f1 -file f2

AppConfig provides a simple method (args()) for parsing command line 
arguments.  A second method (getopt()) allows more complex argument 
processing by delegation to Johan Vroman's Getopt::Long module.

AppConfig also allows variables to be set by parameters passed to a 
CGI script via the URL (GET method).

    http://www.nowhere.com/cgi-bin/myapp?verbose&site=kfs

=head1 PREREQUISITES

AppConfig requires Perl 5.005 or later.  

The L<Getopt::Long> and L<Test::More> modules should be installed.
If you are using a recent version of Perl (e.g. 5.8.0) then these
should already be installed.

=head1 OBTAINING AND INSTALLING THE AppConfig MODULE BUNDLE

The AppConfig module bundle is available from CPAN.  As the 'perlmod' 
manual page explains:

    CPAN stands for the Comprehensive Perl Archive Network.
    This is a globally replicated collection of all known Perl
    materials, including hundreds of unbundled modules.  

    [...]

    For an up-to-date listing of CPAN sites, see
    http://www.perl.com/perl/ or ftp://ftp.perl.com/perl/ .

Within the CPAN archive, AppConfig is in the category:

    12) Option, Argument, Parameter and Configuration File Processing

The module is available in the following directories:

    /modules/by-module/AppConfig/AppConfig-<version>.tar.gz
    /authors/id/ABW/AppConfig-<version>.tar.gz

AppConfig is distributed as a single gzipped tar archive file:

    AppConfig-<version>.tar.gz

Note that "<version>" represents the current AppConfig version
number, of the form "n.nn", e.g. "3.14".  See the REVISION section
below to determine the current version number for AppConfig.

Unpack the archive to create a AppConfig installation directory:

    gunzip AppConfig-<version>.tar.gz
    tar xvf AppConfig-<version>.tar

'cd' into that directory, make, test and install the modules:

    cd AppConfig-<version>
    perl Makefile.PL
    make
    make test
    make install

The 't' sub-directory contains a number of test scripts that are run when 
a 'make test' is run.

The 'make install' will install the module on your system.  You may need 
administrator privileges to perform this task.  If you install the module 
in a local directory (for example, by executing "perl Makefile.PL
LIB=~/lib" in the above - see C<perldoc MakeMaker> for full details), you
will need to ensure that the PERL5LIB environment variable is set to
include the location, or add a line to your scripts explicitly naming the
library location:

    use lib '/local/path/to/lib';

The 'examples' sub-directory contains some simple examples of using the 
AppConfig modules.

=head1 DESCRIPTION

=head2 USING THE AppConfig MODULE

To import and use the L<AppConfig> module the following line should 
appear in your Perl script:

     use AppConfig;

To import constants defined by the AppConfig module, specify the name of
one or more of the constant or tag sets as parameters to C<use>:

    use AppConfig qw(:expand :argcount);

See L<CONSTANT DEFINITIONS> below for more information on the constant
tagsets defined by AppConfig.

AppConfig is implemented using object-oriented methods.  A 
new AppConfig object is created and initialized using the 
new() method.  This returns a reference to a new AppConfig 
object.

    my $config = AppConfig->new();

This will create and return a reference to a new AppConfig object.

In doing so, the AppConfig object also creates an internal reference
to an AppConfig::State object in which to store variable state.  All 
arguments passed into the AppConfig constructor are passed directly
to the AppConfig::State constructor.  

The first (optional) parameter may be a reference to a hash array
containing configuration information.  

    my $config = AppConfig->new( {
            CASE   => 1,
            ERROR  => \&my_error,
            GLOBAL => { 
                    DEFAULT  => "<unset>", 
                    ARGCOUNT => ARGCOUNT_ONE,
                },
        } );

See L<AppConfig::State> for full details of the configuration options
available.  These are, in brief:

=over 4

=item CASE

Used to set case sensitivity for variable names (default: off).

=item CREATE

Used to indicate that undefined variables should be created automatically
(default: off).

=item GLOBAL 

Reference to a hash array of global values used by default when defining 
variables.  Valid global values are DEFAULT, ARGCOUNT, EXPAND, VALIDATE
and ACTION.

=item PEDANTIC

Used to indicate that command line and configuration file parsing routines
should return immediately on encountering an error.

=item ERROR

Used to provide a error handling routine.  Arguments as per printf().

=back

Subsequent parameters may be variable definitions.  These are passed 
to the define() method, described below in L<DEFINING VARIABLES>.

    my $config = AppConfig->new("foo", "bar", "baz");
    my $config = AppConfig->new( { CASE => 1 }, qw(foo bar baz) );

Note that any unresolved method calls to AppConfig are automatically 
delegated to the AppConfig::State object.  In practice, it means that
it is possible to treat the AppConfig object as if it were an 
AppConfig::State object:

    # create AppConfig
    my $config = AppConfig->new('foo', 'bar');

    # methods get passed through to internal AppConfig::State
    $config->foo(100);
    $config->set('bar', 200);
    $config->define('baz');
    $config->baz(300);

=head2 DEFINING VARIABLES

The C<define()> method (delegated to AppConfig::State) is used to 
pre-declare a variable and specify its configuration.

    $config->define("foo");

Variables may also be defined directly from the AppConfig new()
constructor.

    my $config = AppConfig->new("foo");

In both simple examples above, a new variable called "foo" is defined.  A 
reference to a hash array may also be passed to specify configuration 
information for the variable:

    $config->define("foo", {
            DEFAULT   => 99,
            ALIAS     => 'metavar1',
        });

Configuration items specified in the GLOBAL option to the module 
constructor are applied by default when variables are created.  e.g.

    my $config = AppConfig->new( { 
        GLOBAL => {
            DEFAULT  => "<undef>",
            ARGCOUNT => ARGCOUNT_ONE,
        }
    } );

    $config->define("foo");
    $config->define("bar", { ARGCOUNT => ARGCOUNT_NONE } );

is equivalent to:

    my $config = AppConfig->new();

    $config->define( "foo", {
        DEFAULT  => "<undef>",
        ARGCOUNT => ARGCOUNT_ONE,
    } );

    $config->define( "bar", 
        DEFAULT  => "<undef>",
        ARGCOUNT => ARGCOUNT_NONE,
    } );

Multiple variables may be defined in the same call to define().
Configuration hashes for variables can be omitted.

    $config->define("foo", "bar" => { ALIAS = "boozer" }, "baz");

See L<AppConfig::State> for full details of the configuration options
available when defining variables.  These are, in brief:

=over 

=item DEFAULT

The default value for the variable (default: undef).

=item ALIAS

One or more (list reference or "list|like|this") alternative names for the
variable.

=item ARGCOUNT

Specifies the number and type of arguments that the variable expects.
Constants in C<:expand> tag set define ARGCOUNT_NONE - simple on/off flag
(default), ARGCOUNT_ONE - single value, ARGCOUNT_LIST - multiple values
accessed via list reference, ARGCOUNT_HASH - hash table, "key=value",
accessed via hash reference.

=item ARGS 

Used to provide an argument specification string to pass to Getopt::Long 
via AppConfig::Getopt.  E.g. "=i", ":s", "=s@".  This can also be used to 
implicitly set the ARGCOUNT value (C</^!/> = ARGCOUNT_NONE, C</@/> = 
ARGCOUNT_LIST, C</%/> = ARGCOUNT_HASH, C</[=:].*/> = ARGCOUNT_ONE)

=item EXPAND

Specifies which variable expansion policies should be used when parsing 
configuration files.  Constants in C<:expand> tag set define:

    EXPAND_NONE - no expansion (default) 
    EXPAND_VAR  - expand C<$var> or C<$(var)> as other variables
    EXPAND_UID  - expand C<~> and C<~uid> as user's home directory 
    EXPAND_ENV - expand C<${var}> as environment variable
    EXPAND_ALL - do all expansions. 

=item VALIDATE

Regex which the intended variable value should match or code reference 
which returns 1 to indicate successful validation (variable may now be set).

=item ACTION

Code reference to be called whenever variable value changes.

=back

=head2 COMPACT FORMAT DEFINITION

Variables can be specified using a compact format.  This is identical to 
the specification format of Getopt::Long and is of the form:

    "name|alias|alias<argopts>"

The first element indicates the variable name and subsequent ALIAS 
values may be added, each separated by a vertical bar '|'.

The E<lt>argoptsE<gt> element indicates the ARGCOUNT value and may be one of 
the following;

    !                  ARGCOUNT_NONE
    =s                 ARGCOUNT_ONE
    =s@                ARGCOUNT_LIST
    =s%                ARGCOUNT_HASH

Additional constructs supported by Getopt::Long may be specified instead
of the "=s" element (e.g. "=f").  The entire E<lt>argoptsE<gt> element 
is stored in the ARGS parameter for the variable and is passed intact to 
Getopt::Long when the getopt() method is called.  

The following examples demonstrate use of the compact format, with their
equivalent full specifications:

    $config->define("foo|bar|baz!");

    $config->define(
            "foo" => { 
                ALIAS    => "bar|baz", 
                ARGCOUNT => ARGCOUNT_NONE,
            });

    $config->define("name=s");

    $config->define(
            "name" => { 
                ARGCOUNT => ARGCOUNT_ONE,
            });

    $config->define("file|filelist|f=s@");

    $config->define(
            "file" => { 
                ALIAS    => "filelist|f", 
                ARGCOUNT => ARGCOUNT_LIST,
            });

    $config->define("user|u=s%");

    $config->define(
            "user" => { 
                ALIAS    => "u", 
                ARGCOUNT => ARGCOUNT_HASH,
            });

Additional configuration options may be specified by hash reference, as per 
normal.  The compact definition format will override any configuration 
values provided for ARGS and ARGCOUNT.

    $config->define("file|filelist|f=s@", { VALIDATE => \&check_file } );

=head2 READING AND MODIFYING VARIABLE VALUES

AppConfig defines two methods (via AppConfig::State) to manipulate variable 
values

    set($variable, $value);
    get($variable);

Once defined, variables may be accessed directly as object methods where
the method name is the same as the variable name.  i.e.

    $config->set("verbose", 1);

is equivalent to 

    $config->verbose(1); 

Note that AppConfig defines the following methods:

    new();
    file();
    args();
    getopt();

And also, through delegation to AppConfig::State:

    define()
    get()
    set()
    varlist()

If you define a variable with one of the above names, you will not be able
to access it directly as an object method.  i.e.

    $config->file();

This will call the file() method, instead of returning the value of the 
'file' variable.  You can work around this by explicitly calling get() and 
set() on a variable whose name conflicts:

    $config->get('file');

or by defining a "safe" alias by which the variable can be accessed:

    $config->define("file", { ALIAS => "fileopt" });
or
    $config->define("file|fileopt");

    ...
    $config->fileopt();

Without parameters, the current value of the variable is returned.  If
a parameter is specified, the variable is set to that value and the 
result of the set() operation is returned.

    $config->age(29);        # sets 'age' to 29, returns 1 (ok)
    print $config->age();    # prints "29"

The varlist() method can be used to extract a number of variables into
a hash array.  The first parameter should be a regular expression 
used for matching against the variable names. 

    my %vars = $config->varlist("^file");   # all "file*" variables

A second parameter may be specified (any true value) to indicate that 
the part of the variable name matching the regex should be removed 
when copied to the target hash.

    $config->file_name("/tmp/file");
    $config->file_path("/foo:/bar:/baz");

    my %vars = $config->varlist("^file_", 1);

    # %vars:
    #    name => /tmp/file
    #    path => "/foo:/bar:/baz"


=head2 READING CONFIGURATION FILES

The AppConfig module provides a streamlined interface for reading 
configuration files with the AppConfig::File module.  The file() method
automatically loads the AppConfig::File module and creates an object 
to process the configuration file or files.  Variables stored in the 
internal AppConfig::State are automatically updated with values specified 
in the configuration file.  

    $config->file($filename);

Multiple files may be passed to file() and should indicate the file name 
or be a reference to an open file handle or glob.

    $config->file($filename, $filehandle, \*STDIN, ...);

The file may contain blank lines and comments (prefixed by '#') which 
are ignored.  Continutation lines may be marked by ending the line with 
a '\'.

    # this is a comment
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
whatever follows the variable name, up to the end of the current line 
(including any continuation lines).  An optional equals sign may be inserted 
between the variable and value for clarity.

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

    my $beverages = $config->drink();
    print join(", ", @$beverages);      # prints "coffee, tea"

Variables may also be defined as hash lists (ARGCOUNT = ARGCOUNT_HASH).
Each subsequent definition creates a new key and value in the hash array.

    alias l="ls -CF"
    alias e="emacs"

A reference to the hash is returned when the variable is requested.

    my $aliases = $config->alias();
    foreach my $k (keys %$aliases) {
        print "$k => $aliases->{ $k }\n";
    }

The '-' prefix can be used to reset a variable to its default value and
the '+' prefix can be used to set it to 1

    -verbose
    +debug

=head2 VARIABLE EXPANSION

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

=head2 PARSING COMMAND LINE OPTIONS

There are two methods for processing command line options.  The first, 
args(), is a small and efficient implementation which offers basic 
functionality.  The second, getopt(), offers a more powerful and complete
facility by delegating the task to Johan Vroman's Getopt::Long module.  
The trade-off between args() and getopt() is essentially one of speed/size
against flexibility.  Use as appropriate.  Both implement on-demand loading 
of modules and incur no overhead until used.  

The args() method is used to parse simple command line options.  It
automatically loads the AppConfig::Args module and creates an object 
to process the command line arguments.  Variables stored in the internal
AppConfig::State are automatically updated with values specified in the 
arguments.  

The method should be passed a reference to a list of arguments to parse.
The @ARGV array is used if args() is called without parameters.

    $config->args(\@myargs);
    $config->args();               # uses @ARGV

Arguments are read and shifted from the array until the first is
encountered that is not prefixed by '-' or '--'.  At that point, the
method returns 1 to indicate success, leaving any unprocessed arguments
remaining in the list.

Each argument should be the name or alias of a variable prefixed by 
'-' or '--'.  Arguments that are not prefixed as such (and are not an
additional parameter to a previous argument) will cause a warning to be
raised.  If the PEDANTIC option is set, the method will return 0 
immediately.  With PEDANTIC unset (default), the method will continue
to parse the rest of the arguments, returning 0 when done.

If the variable is a simple flag (ARGCOUNT = ARGCOUNT_NONE)
then it is set to the value 1.  The variable may be prefixed by "no" to
set its value to 0.

    myprog -verbose --debug -notaste     # $config->verbose(1)
                                         # $config->debug(1)
                                         # $config->taste(0)

Variables that expect an additional argument (ARGCOUNT != 0) will be set to 
the value of the argument following it.  

    myprog -f /tmp/myfile                # $config->file('/tmp/file');

Variables that expect multiple values (ARGCOUNT = ARGCOUNT_LIST or
ARGCOUNT_HASH) will have successive values added each time the option
is encountered.

    myprog -file /tmp/foo -file /tmp/bar # $config->file('/tmp/foo')
                                         # $config->file('/tmp/bar')

    # file => [ '/tmp/foo', '/tmp/bar' ]

    myprog -door "jim=Jim Morrison" -door "ray=Ray Manzarek"
                                    # $config->door("jim=Jim Morrison");
                                    # $config->door("ray=Ray Manzarek");

    # door => { 'jim' => 'Jim Morrison', 'ray' => 'Ray Manzarek' }

See L<AppConfig::Args> for further details on parsing command line
arguments.

The getopt() method provides a way to use the power and flexibility of
the Getopt::Long module to parse command line arguments and have the 
internal values of the AppConfig object updates automatically.

The first (non-list reference) parameters may contain a number of 
configuration string to pass to Getopt::Long::Configure.  A reference 
to a list of arguments may additionally be passed or @ARGV is used by 
default.

    $config->getopt();                       # uses @ARGV
    $config->getopt(\@myargs);
    $config->getopt(qw(auto_abbrev debug));  # uses @ARGV
    $config->getopt(qw(debug), \@myargs);

See Getopt::Long for details of the configuration options available.

The getopt() method constructs a specification string for each internal
variable and then initializes Getopt::Long with these values.  The
specification string is constructed from the name, any aliases (delimited
by a vertical bar '|') and the value of the ARGS parameter.

    $config->define("foo", {
        ARGS  => "=i",
        ALIAS => "bar|baz",
    });

    # Getopt::Long specification: "foo|bar|baz=i"

Errors and warning generated by the Getopt::Long module are trapped and 
handled by the AppConfig error handler.  This may be a user-defined 
routine installed with the ERROR configuration option.

Please note that the AppConfig::Getopt interface is still experimental
and may not be 100% operational.  This is almost undoubtedly due to 
problems in AppConfig::Getopt rather than Getopt::Long.

=head2 PARSING CGI PARAMETERS

The cgi() method provides an interface to the AppConfig::CGI module
for updating variable values based on the parameters appended to the
URL for a CGI script.  This is commonly known as the CGI 
"GET" method.  The CGI "POST" method is currently not supported.

Parameter definitions are separated from the CGI script name by a 
question mark and from each other by ampersands.  Where variables
have specific values, these are appended to the variable with an 
equals sign:

    http://www.here.com/cgi-bin/myscript?foo=bar&baz=qux&verbose

        # $config->foo('bar');
        # $config->baz('qux');
        # $config->verbose(1);

Certain values specified in a URL must be escaped in the appropriate 
manner (see CGI specifications at http://www.w3c.org/ for full details).  
The AppConfig::CGI module automatically unescapes the CGI query string
to restore the parameters to their intended values.

    http://where.com/mycgi?title=%22The+Wrong+Trousers%22

    # $config->title('"The Wrong Trousers"');

Please be considerate of the security implications of providing writable
access to script variables via CGI.

    http://rebel.alliance.com/cgi-bin/...
        .../send_report?file=%2Fetc%2Fpasswd&email=darth%40empire.com

To avoid any accidental or malicious changing of "private" variables, 
define only the "public" variables before calling the cgi() (or any 
other) method.  Further variables can subsequently be defined which 
can not be influenced by the CGI parameters.

    $config->define('verbose', 'debug')
    $config->cgi();             # can only set verbose and debug

    $config->define('email', 'file');
    $config->file($cfgfile);    # can set verbose, debug, email + file


=head1 CONSTANT DEFINITIONS

A number of constants are defined by the AppConfig module.  These may be
accessed directly (e.g. AppConfig::EXPAND_VARS) or by first importing them
into the caller's package.  Constants are imported by specifying their 
names as arguments to C<use AppConfig> or by importing a set of constants
identified by its "tag set" name.

    use AppConfig qw(ARGCOUNT_NONE ARGCOUNT_ONE);

    use AppConfig qw(:argcount);

The following tag sets are defined:

=over 4

=item :expand

The ':expand' tagset defines the following constants:

    EXPAND_NONE
    EXPAND_VAR
    EXPAND_UID 
    EXPAND_ENV
    EXPAND_ALL       # EXPAND_VAR | EXPAND_UID | EXPAND_ENV
    EXPAND_WARN

See AppConfig::File for full details of the use of these constants.

=item :argcount

The ':argcount' tagset defines the following constants:

    ARGCOUNT_NONE
    ARGCOUNT_ONE
    ARGCOUNT_LIST 
    ARGCOUNT_HASH

See AppConfig::State for full details of the use of these constants.

=back

=head1 REPOSITORY

L<https://github.com/neilbowers/AppConfig>

=head1 AUTHOR

Andy Wardley, E<lt>abw@wardley.orgE<gt>

With contributions from Dave Viner, Ijon Tichy, Axel Gerstmair and
many others whose names have been lost to the sands of time (reminders
welcome).

=head1 COPYRIGHT

Copyright (C) 1997-2007 Andy Wardley.  All Rights Reserved.

Copyright (C) 1997,1998 Canon Research Centre Europe Ltd.

This module is free software; you can redistribute it and/or modify it 
under the same terms as Perl itself.

=head1 SEE ALSO

L<AppConfig::State>, L<AppConfig::File>, L<AppConfig::Args>, L<AppConfig::Getopt>,
L<AppConfig::CGI>, L<Getopt::Long>

=cut
