use strict; use warnings;
package IO::All;
our $VERSION = '0.87';

require Carp;
# So one can use Carp::carp "$message" - without the parenthesis.
sub Carp::carp;

use IO::All::Base -base;

use File::Spec();
use Symbol();
use Fcntl;
use Cwd ();

our @EXPORT = qw(io);

#===============================================================================
# Object creation and setup methods
#===============================================================================
my $autoload = {
    qw(
        touch file

        dir_handle dir
        All dir
        all_files dir
        All_Files dir
        all_dirs dir
        All_Dirs dir
        all_links dir
        All_Links dir
        mkdir dir
        mkpath dir
        next dir

        stdin stdio
        stdout stdio
        stderr stdio

        socket_handle socket
        accept socket
        shutdown socket

        readlink link
        symlink link
    )
};

# XXX - These should die if the given argument exists but is not a
# link, dbm, etc.
sub link  { require IO::All::Link;  goto &IO::All::Link::link; }
sub dbm   { require IO::All::DBM;   goto &IO::All::DBM::dbm; }
sub mldbm { require IO::All::MLDBM; goto &IO::All::MLDBM::mldbm; }

sub autoload { my $self = shift; $autoload; }

sub AUTOLOAD {
    my $self = shift;
    my $method = $IO::All::AUTOLOAD;
    $method =~ s/.*:://;
    my $pkg = ref($self) || $self;
    $self->throw(qq{Can't locate object method "$method" via package "$pkg"})
      if $pkg ne $self->_package;
    my $class = $self->_autoload_class($method);
    my $foo = "$self";
    bless $self, $class;
    $self->$method(@_);
}

sub _autoload_class {
    my $self = shift;
    my $method = shift;
    my $class_id = $self->autoload->{$method} || $method;
    my $ucfirst_class_name = 'IO::All::' . ucfirst($class_id);
    my $ucfirst_class_fn = "IO/All/" . ucfirst($class_id) . ".pm";
    return $ucfirst_class_name if $INC{$ucfirst_class_fn};
    return "IO::All::\U$class_id" if $INC{"IO/All/\U$class_id\E.pm"};
    require IO::All::Temp;
    if (eval "require $ucfirst_class_name; 1") {
        my $class = $ucfirst_class_name;
        my $return = $class->can('new')
        ? $class
        : do { # (OS X hack)
            my $value = $INC{$ucfirst_class_fn};
            delete $INC{$ucfirst_class_fn};
            $INC{"IO/All/\U$class_id\E.pm"} = $value;
            "IO::All::\U$class_id";
        };
        return $return;
    }
    elsif (eval "require IO::All::\U$class_id; 1") {
        return "IO::All::\U$class_id";
    }
    $self->throw("Can't find a class for method '$method'");
}

sub new {
    my $self = shift;
    my $package = ref($self) || $self;
    my $new = bless Symbol::gensym(), $package;
    $new->_package($package);
    $new->_copy_from($self) if ref($self);
    my $name = shift;
    return $name if UNIVERSAL::isa($name, 'IO::All');
    return $new->_init unless defined $name;
    return $new->handle($name)
      if UNIVERSAL::isa($name, 'GLOB') or ref(\ $name) eq 'GLOB';
    # WWW - link is first because a link to a dir returns true for
    # both -l and -d.
    return $new->link($name)	if -l $name;
    return $new->file($name)	if -f $name;
    return $new->dir($name)	if -d $name;
    return $new->$1($name)	if $name =~ /^([a-z]{3,8}):/;
    return $new->socket($name)	if $name =~ /^[\w\-\.]*:\d{1,5}$/;
    return $new->pipe($name)	if $name =~ s/^\s*\|\s*// or $name =~ s/\s*\|\s*$//;
    return $new->string		if $name eq '$';
    return $new->stdio		if $name eq '-';
    return $new->stderr		if $name eq '=';
    return $new->temp		if $name eq '?';
    $new->name($name);
    $new->_init;
}

sub _copy_from {
    my $self = shift;
    my $other = shift;
    for (keys(%{*$other})) {
        # XXX Need to audit exclusions here
        next if /^(_handle|io_handle|is_open)$/;
        *$self->{$_} = *$other->{$_};
    }
}

sub handle {
    my $self = shift;
    $self->_handle(shift) if @_;
    return $self->_init;
}

#===============================================================================
# Overloading support
#===============================================================================
my $old_warn_handler = $SIG{__WARN__};
$SIG{__WARN__} = sub {
    if ($_[0] !~ /^Useless use of .+ \(.+\) in void context/) {
        goto &$old_warn_handler if $old_warn_handler;
        warn(@_);
    }
};

use overload '""'  => '_overload_stringify';
use overload '|'   => '_overload_bitwise_or';
use overload '<<'  => '_overload_left_bitshift';
use overload '>>'  => '_overload_right_bitshift';
use overload '<'   => '_overload_less_than';
use overload '>'   => '_overload_greater_than';
use overload 'cmp' => '_overload_cmp';
use overload '${}' => '_overload_string_deref';
use overload '@{}' => '_overload_array_deref';
use overload '%{}' => '_overload_hash_deref';
use overload '&{}' => '_overload_code_deref';

sub _overload_bitwise_or	{ shift->_overload_handler(@_, '|' ); }
sub _overload_left_bitshift	{ shift->_overload_handler(@_, '<<'); }
sub _overload_right_bitshift	{ shift->_overload_handler(@_, '>>'); }
sub _overload_less_than		{ shift->_overload_handler(@_, '<' ); }
sub _overload_greater_than	{ shift->_overload_handler(@_, '>' ); }
sub _overload_string_deref	{ shift->_overload_handler(@_, '${}'); }
sub _overload_array_deref	{ shift->_overload_handler(@_, '@{}'); }
sub _overload_hash_deref	{ shift->_overload_handler(@_, '%{}'); }
sub _overload_code_deref	{ shift->_overload_handler(@_, '&{}'); }

sub _overload_handler {
    my ($self) = @_;
    my $method = $self->_get_overload_method(@_);
    $self->$method(@_);
}

my $op_swap = {
    '>' => '<', '>>' => '<<',
    '<' => '>', '<<' => '>>',
};

sub _overload_table {
    my $self = shift;
    (
        '* > *' => '_overload_any_to_any',
        '* < *' => '_overload_any_from_any',
        '* >> *' => '_overload_any_addto_any',
        '* << *' => '_overload_any_addfrom_any',

        '* < scalar' => '_overload_scalar_to_any',
        '* > scalar' => '_overload_any_to_scalar',
        '* << scalar' => '_overload_scalar_addto_any',
        '* >> scalar' => '_overload_any_addto_scalar',
    )
};

sub _get_overload_method {
    my ($self, $arg1, $arg2, $swap, $operator) = @_;
    if ($swap) {
        $operator = $op_swap->{$operator} || $operator;
    }
    my $arg1_type = $self->_get_argument_type($arg1);
    my $table1 = { $arg1->_overload_table };

    if ($operator =~ /\{\}$/) {
        my $key = "$operator $arg1_type";
        return $table1->{$key} || $self->_overload_undefined($key);
    }

    my $arg2_type = $self->_get_argument_type($arg2);
    my @table2 = UNIVERSAL::isa($arg2, "IO::All")
    ? ($arg2->_overload_table)
    : ();
    my $table = { %$table1, @table2 };

    my @keys = (
        "$arg1_type $operator $arg2_type",
        "* $operator $arg2_type",
    );
    push @keys, "$arg1_type $operator *", "* $operator *"
      unless $arg2_type =~ /^(scalar|array|hash|code|ref)$/;

    for (@keys) {
        return $table->{$_}
          if defined $table->{$_};
    }

    return $self->_overload_undefined($keys[0]);
}

sub _get_argument_type {
    my $self = shift;
    my $argument = shift;
    my $ref = ref($argument);
    return 'scalar' unless $ref;
    return 'code' if $ref eq 'CODE';
    return 'array' if $ref eq 'ARRAY';
    return 'hash' if $ref eq 'HASH';
    return 'ref' unless $argument->isa('IO::All');
    $argument->file
      if defined $argument->pathname and not $argument->type;
    return $argument->type || 'unknown';
}

sub _overload_cmp {
    my ($self, $other, $swap) = @_;
    $self = defined($self) ? $self.'' : $self;
    ($self, $other) = ($other, $self) if $swap;
    $self cmp $other;
}

sub _overload_stringify {
    my $self = shift;
    my $name = $self->pathname;
    return defined($name) ? $name : overload::StrVal($self);
}

sub _overload_undefined {
    my $self = shift;
    require Carp;
    my $key = shift;
    Carp::carp "Undefined behavior for overloaded IO::All operation: '$key'"
      if $^W;
    return '_overload_noop';
}

sub _overload_noop {
    my $self = shift;
    return;
}

sub _overload_any_addfrom_any {
    $_[1]->append($_[2]->all);
    $_[1];
}

sub _overload_any_addto_any {
    $_[2]->append($_[1]->all);
    $_[2];
}

sub _overload_any_from_any {
    $_[1]->close if $_[1]->is_file and $_[1]->is_open;
    $_[1]->print($_[2]->all);
    $_[1];
}

sub _overload_any_to_any {
    $_[2]->close if $_[2]->is_file and $_[2]->is_open;
    $_[2]->print($_[1]->all);
    $_[2];
}

sub _overload_any_to_scalar {
    $_[2] = $_[1]->all;
}

sub _overload_any_addto_scalar {
    $_[2] .= $_[1]->all;
    $_[2];
}

sub _overload_scalar_addto_any {
    $_[1]->append($_[2]);
    $_[1];
}

sub _overload_scalar_to_any {
    local $\;
    $_[1]->close if $_[1]->is_file and $_[1]->is_open;
    $_[1]->print($_[2]);
    $_[1];
}

#===============================================================================
# Private Accessors
#===============================================================================
field '_package';
field _strict => undef;
field _layers => [];
field _handle => undef;
field _constructor => undef;
field _partial_spec_class => undef;

#===============================================================================
# Public Accessors
#===============================================================================
chain block_size => 1024;
chain errors => undef;
field io_handle => undef;
field is_open => 0;
chain mode => undef;
chain name => undef;
chain perms => undef;
chain separator => $/;
field type => '';

sub _spec_class {
   my $self = shift;

   my $ret = 'File::Spec';
   if (my $partial = $self->_partial_spec_class(@_)) {
      $ret .= '::' . $partial;
      eval "require $ret";
   }

   return $ret
}

sub pathname {my $self = shift; $self->name(@_) }

#===============================================================================
# Chainable option methods (write only)
#===============================================================================
option 'assert';
option 'autoclose' => 1;
option 'backwards';
option 'chomp';
option 'confess';
option 'lock';
option 'rdonly';
option 'rdwr';
option 'strict';

#===============================================================================
# IO::Handle proxy methods
#===============================================================================
proxy 'autoflush';
proxy 'eof';
proxy 'fileno';
proxy 'stat';
proxy 'tell';
proxy 'truncate';

#===============================================================================
# IO::Handle proxy methods that open the handle if needed
#===============================================================================
proxy_open print => '>';
proxy_open printf => '>';
proxy_open sysread => O_RDONLY;
proxy_open syswrite => O_CREAT | O_WRONLY;
proxy_open seek => $^O eq 'MSWin32' ? '<' : '+<';
proxy_open 'getc';

#===============================================================================
# Tie Interface
#===============================================================================
sub tie { my $self = shift; tie *$self, $self; }

sub TIEHANDLE {
    return $_[0] if ref $_[0];
    my $class = shift;
    my $self = bless Symbol::gensym(), $class;
    $self->init(@_);
}

sub READLINE {
    goto &getlines if wantarray;
    goto &getline;
}


sub DESTROY {
    my $self = shift;
    no warnings;
    unless ( $] < 5.008 ) {
        untie *$self if tied *$self;
    }
    $self->close if $self->is_open;
}

sub BINMODE { my $self = shift; CORE::binmode *$self->io_handle; }

{
    no warnings;
    *GETC   = \&getc;
    *PRINT  = \&print;
    *PRINTF = \&printf;
    *READ   = \&read;
    *WRITE  = \&write;
    *SEEK   = \&seek;
    *TELL   = \&getpos;
    *EOF    = \&eof;
    *CLOSE  = \&close;
    *FILENO = \&fileno;
}

#===============================================================================
# File::Spec Interface
#===============================================================================
sub canonpath {
   my $self = shift;
   eval { Cwd::abs_path($self->pathname); 0 } ||
      File::Spec->canonpath($self->pathname)
}

sub catdir {
    my $self = shift;
    my @args = grep defined, $self->name, @_;
    $self->_constructor->()->dir(File::Spec->catdir(@args));
}
sub catfile {
    my $self = shift;
    my @args = grep defined, $self->name, @_;
    $self->_constructor->()->file(File::Spec->catfile(@args));
}
sub join	{ shift->catfile(@_); }
sub curdir	{ shift->_constructor->()->dir(File::Spec->curdir); }
sub devnull	{ shift->_constructor->()->file(File::Spec->devnull); }
sub rootdir	{ shift->_constructor->()->dir(File::Spec->rootdir); }
sub tmpdir	{ shift->_constructor->()->dir(File::Spec->tmpdir); }
sub updir	{ shift->_constructor->()->dir(File::Spec->updir); }
sub case_tolerant{File::Spec->case_tolerant; }
sub is_absolute	{ File::Spec->file_name_is_absolute(shift->pathname); }
sub path	{ my $self = shift; map { $self->_constructor->()->dir($_) } File::Spec->path; }
sub splitpath	{ File::Spec->splitpath(shift->pathname); }
sub splitdir	{ File::Spec->splitdir(shift->pathname); }
sub catpath	{ my $self=shift; $self->_constructor->(File::Spec->catpath(@_)); }
sub abs2rel	{ File::Spec->abs2rel(shift->pathname, @_); }
sub rel2abs	{ File::Spec->rel2abs(shift->pathname, @_); }

#===============================================================================
# Public IO Action Methods
#===============================================================================
sub absolute {
    my $self = shift;
    $self->pathname(File::Spec->rel2abs($self->pathname))
      unless $self->is_absolute;
    $self->is_absolute(1);
    return $self;
}

sub all {
    my $self = shift;
    $self->_assert_open('<');
    local $/;
    my $all = $self->io_handle->getline;
    $self->_error_check;
    $self->_autoclose && $self->close;
    return $all;
}

sub append {
    my $self = shift;
    $self->_assert_open('>>');
    $self->print(@_);
}

sub appendln {
    my $self = shift;
    $self->_assert_open('>>');
    $self->println(@_);
}

sub binary {
    my $self = shift;
    CORE::binmode($self->io_handle) if $self->is_open;
    push @{$self->_layers}, ":raw";
    return $self;
}

sub binmode {
    my $self = shift;
    my $layer = shift;
    $self->_sane_binmode($layer) if $self->is_open;
    push @{$self->_layers}, $layer;
    return $self;
}

sub _sane_binmode {
    my ($self, $layer) = @_;
    $layer
    ? CORE::binmode($self->io_handle, $layer)
    : CORE::binmode($self->io_handle);
}

sub buffer {
    my $self = shift;
    if (not @_) {
        *$self->{buffer} = do {my $x = ''; \ $x}
          unless exists *$self->{buffer};
        return *$self->{buffer};
    }
    my $buffer_ref = ref($_[0]) ? $_[0] : \ $_[0];
    $$buffer_ref = '' unless defined $$buffer_ref;
    *$self->{buffer} = $buffer_ref;
    return $self;
}

sub clear {
    my $self = shift;
    my $buffer = *$self->{buffer};
    $$buffer = '';
    return $self;
}

sub close {
    my $self = shift;
    return unless $self->is_open;
    $self->is_open(0);
    my $io_handle = $self->io_handle;
    $self->io_handle(undef);
    $self->mode(undef);
    $io_handle->close(@_)
      if defined $io_handle;
    return $self;
}

sub empty {
    my $self = shift;
    my $message =
      "Can't call empty on an object that is neither file nor directory";
    $self->throw($message);
}

sub exists {my $self = shift; -e $self->pathname }

sub getline {
    my $self = shift;
    return $self->getline_backwards
      if $self->_backwards;
    $self->_assert_open('<');
    my $line;
    {
        local $/ = @_ ? shift(@_) : $self->separator;
        $line = $self->io_handle->getline;
        chomp($line) if $self->_chomp and defined $line;
    }
    $self->_error_check;
    return $line if defined $line;
    $self->close if $self->_autoclose;
    return undef;
}

sub getlines {
    my $self = shift;
    return $self->getlines_backwards
      if $self->_backwards;
    $self->_assert_open('<');
    my @lines;
    {
        local $/ = @_ ? shift(@_) : $self->separator;
        @lines = $self->io_handle->getlines;
        if ($self->_chomp) {
            chomp for @lines;
        }
    }
    $self->_error_check;
    return @lines if @lines;
    $self->close if $self->_autoclose;
    return ();
}

sub is_dir	{ UNIVERSAL::isa(shift, 'IO::All::Dir');	}
sub is_dbm	{ UNIVERSAL::isa(shift, 'IO::All::DBM');	}
sub is_file	{ UNIVERSAL::isa(shift, 'IO::All::File');	}
sub is_link	{ UNIVERSAL::isa(shift, 'IO::All::Link');	}
sub is_mldbm	{ UNIVERSAL::isa(shift, 'IO::All::MLDBM');	}
sub is_socket	{ UNIVERSAL::isa(shift, 'IO::All::Socket');	}
sub is_stdio	{ UNIVERSAL::isa(shift, 'IO::All::STDIO');	}
sub is_string	{ UNIVERSAL::isa(shift, 'IO::All::String');	}
sub is_temp	{ UNIVERSAL::isa(shift, 'IO::All::Temp');	}
sub length	{ length ${shift->buffer};			}

sub open {
    my $self = shift;
    return $self if $self->is_open;
    $self->is_open(1);
    my ($mode, $perms) = @_;
    $self->mode($mode) if defined $mode;
    $self->mode('<') unless defined $self->mode;
    $self->perms($perms) if defined $perms;
    my @args;
    unless ($self->is_dir) {
        push @args, $self->mode;
        push @args, $self->perms if defined $self->perms;
    }
    if (defined $self->pathname and not $self->type) {
        $self->file;
        return $self->open(@args);
    }
    elsif (defined $self->_handle and
           not $self->io_handle->opened
          ) {
        # XXX Not tested
        $self->io_handle->fdopen($self->_handle, @args);
    }
    $self->_set_binmode;
}

sub println {
    my $self = shift;
    $self->print(map {/\n\z/ ? ($_) : ($_, "\n")} @_);
}

sub read {
    my $self = shift;
    $self->_assert_open('<');
    my $length = (@_ or $self->type eq 'dir')
    ? $self->io_handle->read(@_)
    : $self->io_handle->read(
        ${$self->buffer},
        $self->block_size,
        $self->length,
    );
    $self->_error_check;
    return $length || $self->_autoclose && $self->close && 0;
}

{
    no warnings;
    *readline = \&getline;
}

# deprecated
sub scalar {
    my $self = shift;
    $self->all(@_);
}

sub slurp {
    my $self = shift;
    my $slurp = $self->all;
    return $slurp unless wantarray;
    my $separator = $self->separator;
    if ($self->_chomp) {
        local $/ = $separator;
        map {chomp; $_} split /(?<=\Q$separator\E)/, $slurp;
    }
    else {
        split /(?<=\Q$separator\E)/, $slurp;
    }
}

sub utf8 {
    my $self = shift;
    if ($] < 5.008) {
        die "IO::All -utf8 not supported on Perl older than 5.8";
    }
    $self->encoding('UTF-8');
    return $self;
}

sub _has_utf8 {
    grep { $_ eq ':encoding(UTF-8)' } @{shift->_layers}
}

sub encoding {
    my $self = shift;
    my $encoding = shift;
    if ($] < 5.008) {
        die "IO::All -encoding not supported on Perl older than 5.8";
    }
    die "No valid encoding string sent" if !$encoding;
    $self->_set_encoding($encoding) if $self->is_open and $encoding;
    push @{$self->_layers}, ":encoding($encoding)";
    return $self;
}

sub _set_encoding {
    my ($self, $encoding) = @_;
    return CORE::binmode($self->io_handle, ":encoding($encoding)");
}

sub write {
    my $self = shift;
    $self->_assert_open('>');
    my $length = @_
    ? $self->io_handle->write(@_)
    : $self->io_handle->write(${$self->buffer}, $self->length);
    $self->_error_check;
    $self->clear unless @_;
    return $length;
}

#===============================================================================
# Implementation methods. Subclassable.
#===============================================================================
sub throw {
    my $self = shift;
    require Carp;
    ;
    return &{$self->errors}(@_)
      if $self->errors;
    return Carp::confess(@_)
      if $self->_confess;
    return Carp::croak(@_);
}

#===============================================================================
# Private instance methods
#===============================================================================
sub _assert_dirpath {
    my $self = shift;
    my $dir_name = shift;
    return $dir_name if ((! CORE::length($dir_name)) or
      -d $dir_name or
      CORE::mkdir($dir_name, $self->perms || 0755) or
      do {
          require File::Path;
          File::Path::mkpath($dir_name, 0, $self->perms || 0755 );
      } or
      $self->throw("Can't make $dir_name"));
}

sub _assert_open {
    my $self = shift;
    return if $self->is_open;
    $self->file unless $self->type;
    return $self->open(@_);
}

sub _error_check {
    my $self = shift;
    my $saved_error = $!;
    return unless $self->io_handle->can('error');
    return unless $self->io_handle->error;
    $self->throw($saved_error);
}

sub _set_binmode {
    my $self = shift;
    $self->_sane_binmode($_) for @{$self->_layers};
    return $self;
}

#===============================================================================
# Stat Methods
#===============================================================================
BEGIN {
    no strict 'refs';
    my @stat_fields = qw(
        device inode modes nlink uid gid device_id size atime mtime
        ctime blksize blocks
    );
    foreach my $stat_field_idx (0 .. $#stat_fields)
    {
        my $idx = $stat_field_idx;
        my $name = $stat_fields[$idx];

        *$name = sub {
            my $self = shift;
            return (stat($self->io_handle || $self->pathname))[$idx];
        };
    }
}

