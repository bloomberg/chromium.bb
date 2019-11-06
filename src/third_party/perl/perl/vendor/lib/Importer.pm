package Importer;
use strict qw/vars subs/; # Not refs!
use warnings; no warnings 'once';

our $VERSION = '0.025';

my %SIG_TO_SLOT = (
    '&' => 'CODE',
    '$' => 'SCALAR',
    '%' => 'HASH',
    '@' => 'ARRAY',
    '*' => 'GLOB',
);

our %IMPORTED;

# This will be used to check if an import arg is a version number
my %NUMERIC = map +($_ => 1), 0 .. 9;

sub IMPORTER_MENU() {
    return (
        export_ok   => [qw/optimal_import/],
        export_anon => {
            import => sub {
                my $from  = shift;
                my @caller = caller(0);

                _version_check($from, \@caller, shift @_) if @_ && $NUMERIC{substr($_[0], 0, 1)};

                my $file = _mod_to_file($from);
                _load_file(\@caller, $file) unless $INC{$file};

                return if optimal_import($from, $caller[0], \@caller, @_);

                my $self = __PACKAGE__->new(
                    from   => $from,
                    caller => \@caller,
                );

                $self->do_import($caller[0], @_);
            },
        },
    );
}

###########################################################################
#
# These are class methods
# import and unimport are what you would expect.
# import_into and unimport_from are the indirect forms you can use in other
# package import() methods.
#
# These all attempt to do a fast optimal-import if possible, then fallback to
# the full-featured import that constructs an object when needed.
#

sub import {
    my $class = shift;

    my @caller = caller(0);

    _version_check($class, \@caller, shift @_) if @_ && $NUMERIC{substr($_[0], 0, 1)};

    return unless @_;

    my ($from, @args) = @_;

    my $file = _mod_to_file($from);
    _load_file(\@caller, $file) unless $INC{$file};

    return if optimal_import($from, $caller[0], \@caller, @args);

    my $self = $class->new(
        from   => $from,
        caller => \@caller,
    );

    $self->do_import($caller[0], @args);
}

sub unimport {
    my $class = shift;
    my @caller = caller(0);

    my $self = $class->new(
        from   => $caller[0],
        caller => \@caller,
    );

    $self->do_unimport(@_);
}

sub import_into {
    my $class = shift;
    my ($from, $into, @args) = @_;

    my @caller;

    if (ref($into)) {
        @caller = @$into;
        $into = $caller[0];
    }
    elsif ($into =~ m/^\d+$/) {
        @caller = caller($into + 1);
        $into = $caller[0];
    }
    else {
        @caller = caller(0);
    }

    my $file = _mod_to_file($from);
    _load_file(\@caller, $file) unless $INC{$file};

    return if optimal_import($from, $into, \@caller, @args);

    my $self = $class->new(
        from   => $from,
        caller => \@caller,
    );

    $self->do_import($into, @args);
}

sub unimport_from {
    my $class = shift;
    my ($from, @args) = @_;

    my @caller;
    if ($from =~ m/^\d+$/) {
        @caller = caller($from + 1);
        $from = $caller[0];
    }
    else {
        @caller = caller(0);
    }

    my $self = $class->new(
        from   => $from,
        caller => \@caller,
    );

    $self->do_unimport(@args);
}

###########################################################################
#
# Constructors
#

sub new {
    my $class = shift;
    my %params = @_;

    my $caller = $params{caller} || [caller()];

    die "You must specify a package to import from at $caller->[1] line $caller->[2].\n"
        unless $params{from};

    return bless {
        from   => $params{from},
        caller => $params{caller},    # Do not use our caller.
    }, $class;
}

###########################################################################
#
# Shortcuts for getting symbols without any namespace modifications
#

sub get {
    my $proto = shift;
    my @caller = caller(1);

    my $self = ref($proto) ? $proto : $proto->new(
        from   => shift(@_),
        caller => \@caller,
    );

    my %result;
    $self->do_import($caller[0], @_, sub { $result{$_[0]} = $_[1] });
    return \%result;
}

sub get_list {
    my $proto = shift;
    my @caller = caller(1);

    my $self = ref($proto) ? $proto : $proto->new(
        from   => shift(@_),
        caller => \@caller,
    );

    my @result;
    $self->do_import($caller[0], @_, sub { push @result => $_[1] });
    return @result;
}

sub get_one {
    my $proto = shift;
    my @caller = caller(1);

    my $self = ref($proto) ? $proto : $proto->new(
        from   => shift(@_),
        caller => \@caller,
    );

    my $result;
    $self->do_import($caller[0], @_, sub { $result = $_[1] });
    return $result;
}

###########################################################################
#
# Object methods
#

sub do_import {
    my $self = shift;

    my ($into, $versions, $exclude, $import, $set) = $self->parse_args(@_);

    # Exporter supported multiple version numbers being listed...
    _version_check($self->from, $self->get_caller, @$versions) if @$versions;

    return unless @$import;

    $self->_handle_fail($into, $import) if $self->menu($into)->{fail};
    $self->_set_symbols($into, $exclude, $import, $set);
}

sub do_unimport {
    my $self = shift;

    my $from = $self->from;
    my $imported = $IMPORTED{$from} or $self->croak("'$from' does not have any imports to remove");

    my %allowed = map { $_ => 1 } @$imported;

    my @args = @_ ? @_ : @$imported;

    my $stash = \%{"$from\::"};

    for my $name (@args) {
        $name =~ s/^&//;

        $self->croak("Sub '$name' was not imported using " . ref($self)) unless $allowed{$name};

        my $glob = delete $stash->{$name};
        local *GLOBCLONE = *$glob;

        for my $type (qw/SCALAR HASH ARRAY FORMAT IO/) {
            next unless defined(*{$glob}{$type});
            *{"$from\::$name"} = *{$glob}{$type}
        }
    }
}

sub from { $_[0]->{from} }

sub from_file {
    my $self = shift;

    $self->{from_file} ||= _mod_to_file($self->{from});

    return $self->{from_file};
}

sub load_from {
    my $self = shift;
    my $from_file = $self->from_file;
    my $this_file = __FILE__;

    return if $INC{$from_file};

    my $caller = $self->get_caller;

    _load_file($caller, $from_file);
}

sub get_caller {
    my $self = shift;
    return $self->{caller} if $self->{caller};

    my $level = 1;
    while(my @caller = caller($level++)) {
        return \@caller if @caller && !$caller[0]->isa(__PACKAGE__);
        last unless @caller;
    }

    # Fallback
    return [caller(0)];
}

sub croak {
    my $self = shift;
    my ($msg) = @_;
    my $caller = $self->get_caller;
    my $file = $caller->[1] || 'unknown file';
    my $line = $caller->[2] || 'unknown line';
    die "$msg at $file line $line.\n";
}

sub carp {
    my $self = shift;
    my ($msg) = @_;
    my $caller = $self->get_caller;
    my $file = $caller->[1] || 'unknown file';
    my $line = $caller->[2] || 'unknown line';
    warn "$msg at $file line $line.\n";
}

sub menu {
    my $self = shift;
    my ($into) = @_;

    $self->croak("menu() requires the name of the destination package")
        unless $into;

    my $for = $self->{menu_for};
    delete $self->{menu} if $for && $for ne $into;
    return $self->{menu} || $self->reload_menu($into);
}

sub reload_menu {
    my $self = shift;
    my ($into) = @_;

    $self->croak("reload_menu() requires the name of the destination package")
        unless $into;

    my $from = $self->from;

    if (my $menu_sub = *{"$from\::IMPORTER_MENU"}{CODE}) {
        # Hook, other exporter modules can define this method to be compatible with
        # Importer.pm

        my %got = $from->$menu_sub($into, $self->get_caller);

        $got{export}       ||= [];
        $got{export_ok}    ||= [];
        $got{export_tags}  ||= {};
        $got{export_fail}  ||= [];
        $got{export_anon}  ||= {};
        $got{export_magic} ||= {};

        $self->croak("'$from' provides both 'generate' and 'export_gen' in its IMPORTER_MENU (They are exclusive, module must pick 1)")
            if $got{export_gen} && $got{generate};

        $got{export_gen} ||= {};

        $self->{menu} = $self->_build_menu($into => \%got, 1);
    }
    else {
        my %got;
        $got{export}        = \@{"$from\::EXPORT"};
        $got{export_ok}     = \@{"$from\::EXPORT_OK"};
        $got{export_tags}   = \%{"$from\::EXPORT_TAGS"};
        $got{export_fail}   = \@{"$from\::EXPORT_FAIL"};
        $got{export_gen}    = \%{"$from\::EXPORT_GEN"};
        $got{export_anon}   = \%{"$from\::EXPORT_ANON"};
        $got{export_magic}  = \%{"$from\::EXPORT_MAGIC"};

        $self->{menu} = $self->_build_menu($into => \%got, 0);
    }

    $self->{menu_for} = $into;

    return $self->{menu};
}

sub _build_menu {
    my $self = shift;
    my ($into, $got, $new_style) = @_;

    my $from = $self->from;

    my $export       = $got->{export}       || [];
    my $export_ok    = $got->{export_ok}    || [];
    my $export_tags  = $got->{export_tags}  || {};
    my $export_fail  = $got->{export_fail}  || [];
    my $export_anon  = $got->{export_anon}  || {};
    my $export_gen   = $got->{export_gen}   || {};
    my $export_magic = $got->{export_magic} || {};

    my $generate = $got->{generate};

    $generate ||= sub {
        my $symbol = shift;
        my ($sig, $name) = ($symbol =~ m/^(\W?)(.*)$/);
        $sig ||= '&';

        my $do = $export_gen->{"${sig}${name}"};
        $do ||= $export_gen->{$name} if !$sig || $sig eq '&';

        return undef unless $do;

        $from->$do($into, $symbol);
    } if $export_gen && keys %$export_gen;

    my $lookup  = {};
    my $exports = {};
    for my $sym (@$export, @$export_ok, keys %$export_gen, keys %$export_anon) {
        my ($sig, $name) = ($sym =~ m/^(\W?)(.*)$/);
        $sig ||= '&';

        $lookup->{"${sig}${name}"} = 1;
        $lookup->{$name} = 1 if $sig eq '&';

        next if $export_gen->{"${sig}${name}"};
        next if $sig eq '&' && $export_gen->{$name};
        next if $got->{generate} && $generate->("${sig}${name}");

        my $fqn = "$from\::$name";
        # We cannot use *{$fqn}{TYPE} here, it breaks for autoloaded subs, this
        # does not:
        $exports->{"${sig}${name}"} = $export_anon->{$sym} || (
            $sig eq '&' ? \&{$fqn} :
            $sig eq '$' ? \${$fqn} :
            $sig eq '@' ? \@{$fqn} :
            $sig eq '%' ? \%{$fqn} :
            $sig eq '*' ? \*{$fqn} :
            # Sometimes people (CGI::Carp) put invalid names (^name=) into
            # @EXPORT. We simply go to 'next' in these cases. These modules
            # have hooks to prevent anyone actually trying to import these.
            next
        );
    }

    my $f_import = $new_style || $from->can('import');
    $self->croak("'$from' does not provide any exports")
        unless $new_style
            || keys %$exports
            || $from->isa('Exporter')
            || ($INC{'Exporter.pm'} && $f_import && $f_import == \&Exporter::import);

    # Do not cleanup or normalize the list added to the DEFAULT tag, legacy....
    my $tags = {
        %$export_tags,
        'DEFAULT' => [ @$export ],
    };

    # Add 'ALL' tag unless already specified. We want to normalize it.
    $tags->{ALL} ||= [ sort grep {m/^[\&\$\@\%\*]/} keys %$lookup ];

    my $fail = @$export_fail ? {
        map {
            my ($sig, $name) = (m/^(\W?)(.*)$/);
            $sig ||= '&';
            ("${sig}${name}" => 1, $sig eq '&' ? ($name => 1) : ())
        } @$export_fail
    } : undef;

    my $menu = {
        lookup   => $lookup,
        exports  => $exports,
        tags     => $tags,
        fail     => $fail,
        generate => $generate,
        magic    => $export_magic,
    };

    return $menu;
}

sub parse_args {
    my $self = shift;
    my ($into, @args) = @_;

    my $menu = $self->menu($into);

    my @out = $self->_parse_args($into, $menu, \@args);
    pop @out;
    return @out;
}

sub _parse_args {
    my $self = shift;
    my ($into, $menu, $args, $is_tag) = @_;

    my $from = $self->from;
    my $main_menu = $self->menu($into);
    $menu ||= $main_menu;

    # First we strip out versions numbers and setters, this simplifies the logic late.
    my @sets;
    my @versions;
    my @leftover;
    for my $arg (@$args) {
        no warnings 'void';

        # Code refs are custom setters
        # If the first character is an ASCII numeric then it is a version number
        push @sets     => $arg and next if ref($arg) eq 'CODE';
        push @versions => $arg xor next if $NUMERIC{substr($arg, 0, 1)};
        push @leftover => $arg;
    }

    $self->carp("Multiple setters specified, only 1 will be used") if @sets > 1;
    my $set = pop @sets;

    $args = \@leftover;
    @$args = (':DEFAULT') unless $is_tag || @$args || @versions;

    my %exclude;
    my @import;

    while(my $full_arg = shift @$args) {
        my $arg = $full_arg;
        my $lead = substr($arg, 0, 1);

        my ($spec, $exc);
        if ($lead eq '!') {
            $exc = $lead;

            if ($arg eq '!') {
                # If the current arg is just '!' then we are negating the next item.
                $arg = shift @$args;
            }
            else {
                # Strip off the '!'
                substr($arg, 0, 1, '');
            }

            # Exporter.pm legacy behavior
            # negated first item implies starting with default set:
            unshift @$args => ':DEFAULT' unless @import || keys %exclude || @versions;

            # Now we have a new lead character
            $lead = substr($arg, 0, 1);
        }
        else {
            # If the item is followed by a reference then they are asking us to
            # do something special...
            $spec = ref($args->[0]) eq 'HASH' ? shift @$args : {};
        }

        if($lead eq ':') {
            substr($arg, 0, 1, '');
            my $tag = $menu->{tags}->{$arg} or $self->croak("$from does not export the :$arg tag");

            my (undef, $cvers, $cexc, $cimp, $cset, $newmenu) = $self->_parse_args($into, $menu, $tag, $arg);

            $self->croak("Exporter specified version numbers (" . join(', ', @$cvers) . ") in the :$arg tag!")
                if @$cvers;

            $self->croak("Exporter specified a custom symbol setter in the :$arg tag!")
                if $cset;

            # Merge excludes
            %exclude = (%exclude, %$cexc);

            if ($exc) {
                $exclude{$_} = 1 for grep {!ref($_) && substr($_, 0, 1) ne '+'} map {$_->[0]} @$cimp;
            }
            elsif ($spec && keys %$spec) {
                $self->croak("Cannot use '-as' to rename multiple symbols included by: $full_arg")
                    if $spec->{'-as'} && @$cimp > 1;

                for my $set (@$cimp) {
                    my ($sym, $cspec) = @$set;

                    # Start with a blind squash, spec from tag overrides the ones inside.
                    my $nspec = {%$cspec, %$spec};

                    $nspec->{'-prefix'}  = "$spec->{'-prefix'}$cspec->{'-prefix'}"   if $spec->{'-prefix'}  && $cspec->{'-prefix'};
                    $nspec->{'-postfix'} = "$cspec->{'-postfix'}$spec->{'-postfix'}" if $spec->{'-postfix'} && $cspec->{'-postfix'};

                    push @import => [$sym, $nspec];
                }
            }
            else {
                push @import => @$cimp;
            }

            # New menu
            $menu = $newmenu;

            next;
        }

        # Process the item to figure out what symbols are being touched, if it
        # is a tag or regex than it can be multiple.
        my @list;
        if(ref($arg) eq 'Regexp') {
            @list = sort grep /$arg/, keys %{$menu->{lookup}};
        }
        elsif($lead eq '/' && $arg =~ m{^/(.*)/$}) {
            my $pattern = $1;
            @list = sort grep /$1/, keys %{$menu->{lookup}};
        }
        else {
            @list = ($arg);
        }

        # Normalize list, always have a sigil
        @list = map {m/^\W/ ? $_ : "\&$_" } @list;

        if ($exc) {
            $exclude{$_} = 1 for @list;
        }
        else {
            $self->croak("Cannot use '-as' to rename multiple symbols included by: $full_arg")
                if $spec->{'-as'} && @list > 1;

            push @import => [$_, $spec] for @list;
        }
    }

    return ($into, \@versions, \%exclude, \@import, $set, $menu);
}

sub _handle_fail {
    my $self = shift;
    my ($into, $import) = @_;

    my $from = $self->from;
    my $menu = $self->menu($into);

    # Historically Exporter would strip the '&' off of sub names passed into export_fail.
    my @fail = map {my $x = $_->[0]; $x =~ s/^&//; $x} grep $menu->{fail}->{$_->[0]}, @$import or return;

    my @real_fail = $from->can('export_fail') ? $from->export_fail(@fail) : @fail;

    if (@real_fail) {
        $self->carp(qq["$_" is not implemented by the $from module on this architecture])
            for @real_fail;

        $self->croak("Can't continue after import errors");
    }

    $self->reload_menu($menu);
    return;
}

sub _set_symbols {
    my $self = shift;
    my ($into, $exclude, $import, $custom_set) = @_;

    my $from   = $self->from;
    my $menu   = $self->menu($into);
    my $caller = $self->get_caller();

    my $set_symbol = $custom_set || eval <<"    EOT" || die $@;
# Inherit the callers warning settings. If they have warnings and we
# redefine their subs they will hear about it. If they do not have warnings
# on they will not.
BEGIN { \${^WARNING_BITS} = \$caller->[9] if defined \$caller->[9] }
#line $caller->[2] "$caller->[1]"
sub { *{"$into\\::\$_[0]"} = \$_[1] }
    EOT

    for my $set (@$import) {
        my ($symbol, $spec) = @$set;

        my ($sig, $name) = ($symbol =~ m/^(\W)(.*)$/) or die "Invalid symbol: $symbol";

        # Find the thing we are actually shoving in a new namespace
        my $ref = $menu->{exports}->{$symbol};
        $ref ||= $menu->{generate}->($symbol) if $menu->{generate};

        # Exporter.pm supported listing items in @EXPORT that are not actually
        # available for export. So if it is listed (lookup) but nothing is
        # there (!$ref) we simply skip it.
        $self->croak("$from does not export $symbol") unless $ref || $menu->{lookup}->{"${sig}${name}"};
        next unless $ref;

        my $type = ref($ref);
        $type = 'SCALAR' if $type eq 'REF';
        $self->croak("Symbol '$sig$name' requested, but reference (" . ref($ref) . ") does not match sigil ($sig)")
            if $ref && $type ne $SIG_TO_SLOT{$sig};

        # If they directly renamed it then we assume they want it under the new
        # name, otherwise excludes get kicked. It is useful to be able to
        # exclude an item in a tag/match where the group has a prefix/postfix.
        next if $exclude->{"${sig}${name}"} && !$spec->{'-as'};

        my $new_name = join '' => ($spec->{'-prefix'} || '', $spec->{'-as'} || $name, $spec->{'-postfix'} || '');

        # Set the symbol (finally!)
        $set_symbol->($new_name, $ref, sig => $sig, symbol => $symbol, into => $into, from => $from, spec => $spec);

        # The remaining things get skipped with a custom setter
        next if $custom_set;

        # Record the import so that we can 'unimport'
        push @{$IMPORTED{$into}} => $new_name if $sig eq '&';

        # Apply magic
        my $magic = $menu->{magic}->{$symbol};
        $magic  ||= $menu->{magic}->{$name} if $sig eq '&';
        $from->$magic(into => $into, orig_name => $name, new_name => $new_name, ref => $ref)
            if $magic;
    }
}

###########################################################################
#
# The rest of these are utility functions, not methods!
#

sub _version_check {
    my ($mod, $caller, @versions) = @_;

    eval <<"    EOT" or die $@;
#line $caller->[2] "$caller->[1]"
\$mod->VERSION(\$_) for \@versions;
1;
    EOT
}

sub _mod_to_file {
    my $file = shift;
    $file =~ s{::}{/}g;
    $file .= '.pm';
    return $file;
}

sub _load_file {
    my ($caller, $file) = @_;

    eval <<"    EOT" || die $@;
#line $caller->[2] "$caller->[1]"
require \$file;
    EOT
}


my %HEAVY_VARS = (
    IMPORTER_MENU => 'CODE',     # Origin package has a custom menu
    EXPORT_FAIL   => 'ARRAY',    # Origin package has a failure handler
    EXPORT_GEN    => 'HASH',     # Origin package has generators
    EXPORT_ANON   => 'HASH',     # Origin package has anonymous exports
    EXPORT_MAGIC  => 'HASH',     # Origin package has magic to apply post-export
);

sub optimal_import {
    my ($from, $into, $caller, @args) = @_;

    defined(*{"$from\::$_"}{$HEAVY_VARS{$_}}) and return 0 for keys %HEAVY_VARS;

    # Default to @EXPORT
    @args = @{"$from\::EXPORT"} unless @args;

    # Subs will be listed without sigil in %allowed, all others keep sigil
    my %allowed = map +(substr($_, 0, 1) eq '&' ? substr($_, 1) : $_ => 1),
        @{"$from\::EXPORT"}, @{"$from\::EXPORT_OK"};

    # First check if it is allowed, stripping '&' if necessary, which will also
    # let scalars in, we will deal with those shortly.
    # If not allowed return 0 (need to do a heavy import)
    # if it is allowed then see if it has a CODE slot, if so use it, otherwise
    # we have a symbol that needs heavy due to non-sub, autoload, etc.
    # This will not allow $foo to import foo() since '$from' still contains the
    # sigil making it an invalid symbol name in our globref below.
    my %final = map +(
        (!ref($_) && ($allowed{$_} || (substr($_, 0, 1, "") eq '&' && $allowed{$_})))
            ? ($_ => *{"$from\::$_"}{CODE} || return 0)
            : return 0
    ), @args;

    eval <<"    EOT" || die $@;
# If the caller has redefine warnings enabled then we want to warn them if
# their import redefines things.
BEGIN { \${^WARNING_BITS} = \$caller->[9] if defined \$caller->[9] };
#line $caller->[2] "$caller->[1]"
(*{"$into\\::\$_"} = \$final{\$_}, push \@{\$Importer::IMPORTED{\$into}} => \$_) for keys %final;
1;
    EOT
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Importer - Alternative but compatible interface to modules that export symbols.

=head1 DESCRIPTION

This module acts as a layer between L<Exporter> and modules which consume
exports. It is feature-compatible with L<Exporter>, plus some much needed
extras. You can use this to import symbols from any exporter that follows
L<Exporter>s specification. The exporter modules themselves do not need to use
or inherit from the L<Exporter> module, they just need to set C<@EXPORT> and/or
other variables.

=head1 SYNOPSIS

    # Import defaults
    use Importer 'Some::Module';

    # Import a list
    use Importer 'Another::Module' => qw/foo bar baz/;

    # Import a specific version:
    use Importer 'That::Module' => '1.00';

    # Require a sepcific version of Importer
    use Importer 0.001, 'Foo::Bar' => qw/a b c/;

    foo()
    bar()
    baz()

    # Remove all subroutines imported by Importer
    no Importer;

    # Import symbols into variables
    my $croak = Importer->get_one(Carp => qw/croak/);
    $croak->("This will croak");

    my $CARP = Importer->get(Carp => qw/croak confess cluck/);
    $CARP->{croak}->("This will croak");
    $CARP->{cluck}->("This will cluck");
    $CARP->{confess}->("This will confess");

=head1 WHY?

There was recently a discussion on p5p about adding features to L<Exporter>.
This conversation raised some significant concerns, those are listed here, in
addition to others.

=over 4

=item The burden is on export consumers to specify a version of Exporter

Adding a feature to L<Exporter> means that any consumer module that relies on
the new features must depend on a specific version of L<Exporter>. This seems
somewhat backwards since L<Exporter> is used by the module you are importing
from.

=item Exporter.pm is really old/crazy code

Not much more to say here. It is very old, it is very crazy, and if you break
it you break EVERYTHING.

=item Using a modules import() for exporting makes it hard to give it other purposes

It is not unusual for a module to want to export symbols and provide import
behaviors. It is also not unusual for a consumer to only want 1 or the other.
Using this module you can import symbols without also getting the C<import()>
side effects.

In addition, moving forward, modules can specify exports and have a custom
C<import()> without conflating the two. A module can tell you to use Importer
to get the symbols, and to use the module directly for behaviors. A module
could also use Importer within its own C<import()> method without the need to
subclass L<Exporter>, or bring in its C<import()> method.

=item There are other exporter modules on cpan

This module normally assumes an exporter uses L<Exporter>, so it looks for the
variables and methods L<Exporter> expects. However, other exporters on cpan can
override this using the C<IMPORTER_MENU()> hook.

=back

=head1 COMPATIBILITY

This module aims for 100% compatibility with every feature of L<Exporter>, plus
added features such as import renaming.

If you find something that works differently, or not at all when compared to
L<Exporter> please report it as a bug, unless it is noted as an intentional
feature (like import renaming).

=head1 IMPORT PARAMETERS

    use Importer $IMPORTER_VERSION, $FROM_MODULE, $FROM_MODULE_VERSION, \&SET_SYMBOL, @SYMBOLS;

=over 4

=item $IMPORTER_VERSION (optional)

If you provide a numeric argument as the first argument it will be treated as a
version number. Importer will do a version check to make sure it is at least at
the requested version.

=item $FROM_MODULE (required)

This is the only required argument. This is the name of the module to import
symbols from.

=item $FROM_MODULE_VERSION (optional)

Any numeric argument following the C<$FROM_MODULE> will be treated as a version
check against C<$FROM_MODULE>.

=item \&SET_SYMBOL (optional)

Normally Importer will put the exports into your namespace. This is usually
done via a more complex form of C<*name = $ref>. If you do NOT want this to
happen then you can provide a custom sub to handle the assignment.

This is an example that uses this feature to put all the exports into a lexical
hash instead of modifying the namespace (This is how the C<get()> method is
implemented).

    my %CARP;
    use Importer Carp => sub {
        my ($name, $ref) = @_;
        $CARP{$name} = $ref;
    };

    $CARP{cluck}->("This will cluck");
    $CARP{croak}->("This will croak");

The first two arguments to the custom sub are the name (no sigil), and the
reference. The additional arguments are key/value pairs:

    sub set_symbol {
        my ($name, $ref, %info) = @_;
    }

=over 4

=item $info{from}

Package the symbol comes from.

=item $info{into}

Package to which the symbol should be added.

=item $info{sig}

The sigil that should be used.

=item $info{spec}

Extra details.

=item $info{symbol}

The original symbol name (with sigil) from the original package.

=back

=item @SYMBOLS (optional)

Symbols you wish to import. If no symbols are specified then the defaults will
be used. You may also specify tags using the ':' prefix.

=back

=head1 SUPPORTED FEATURES

=head2 TAGS

You can define/import subsets of symbols using predefined tags.

    use Importer 'Some::Thing' => ':tag';

L<Importer> will automatically populate the C<:DEFAULT> tag for you.
L<Importer> will also give you an C<:ALL> tag with ALL exports so long as the
exporter does not define a C<:ALL> tag already.

=head2 /PATTERN/ or qr/PATTERN/

You can import all symbols that match a pattern. The pattern can be supplied a
string starting and ending with '/', or you can provide a C<qr/../> reference.

    use Importer 'Some::Thing' => '/oo/';

    use Importer 'Some::Thing' => qr/oo/;

=head2 EXCLUDING SYMBOLS

You can exclude symbols by prefixing them with '!'.

    use Importer 'Some::Thing'
        '!foo',         # Exclude one specific symbol
        '!/pattern/',   # Exclude all matching symbols
        '!' => qr/oo/,  # Exclude all that match the following arg
        '!:tag';        # Exclude all in tag

=head2 RENAMING SYMBOLS AT IMPORT

I<This is a new feature,> L<Exporter> I<does not support this on its own.>

You can rename symbols at import time using a specification hash following the
import name:

    use Importer 'Some::Thing' => (
        foo => { -as => 'my_foo' },
    );

You can also add a prefix and/or postfix:

    use Importer 'Some::Thing' => (
        foo => { -prefix => 'my_' },
    );

Using this syntax to set prefix and/or postfix also works on tags and patterns
that are specified for import, in which case the prefix/postfix is applied to
all symbols from the tag/patterm.

=head2 CUSTOM EXPORT ASSIGNMENT

This lets you provide an alternative to the C<*name = $ref> export assignment.
See the list of L<parameters|/"IMPORT PARAMETERS"> to C<import()>

=head2 UNIMPORTING

See L</UNIMPORT PARAMETERS>.

=head2 ANONYMOUS EXPORTS

See L</%EXPORT_ANON>.

=head2 GENERATED EXPORTS

See L</%EXPORT_GEN>.

=head1 UNIMPORT PARAMETERS

    no Importer;    # Remove all subs brought in with Importer

    no Importer qw/foo bar/;    # Remove only the specified subs

B<Only subs can be unimported>.

B<You can only unimport subs imported using Importer>.

=head1 SUPPORTED VARIABLES

=head2 @EXPORT

This is used exactly the way L<Exporter> uses it.

List of symbols to export. Sigil is optional for subs. Symbols listed here are
exported by default. If possible you should put symbols in C<@EXPORT_OK>
instead.

    our @EXPORT = qw/foo bar &baz $BAT/;

=head2 @EXPORT_OK

This is used exactly the way L<Exporter> uses it.

List of symbols that can be imported. Sigil is optional for subs. Symbols
listed here are not exported by default. This is preferred over C<@EXPORT>.

    our @EXPORT_OK = qw/foo bar &baz $BAT/;

=head2 %EXPORT_TAGS

This module supports tags exactly the way L<Exporter> does.

    use Importer 'Some::Thing'  => ':DEFAULT';

    use Importer 'Other::Thing' => ':some_tag';

Tags can be specified this way:

    our %EXPORT_TAGS = (
        oos => [qw/foo boo zoo/],
        ees => [qw/fee bee zee/],
    );

=head2 @EXPORT_FAIL

This is used exactly the way L<Exporter> uses it.

Use this to list subs that are not available on all platforms. If someone tries
to import one of these, Importer will hit your C<< $from->export_fail(@items) >>
callback to try to resolve the issue. See L<Exporter> for documentation of
this feature.

    our @EXPORT_FAIL = qw/maybe_bad/;

=head2 %EXPORT_ANON

This is new to this module, L<Exporter> does not support it.

This allows you to export symbols that are not actually in your package symbol
table. The keys should be the symbol names, the values are the references for
the symbols.

    our %EXPORT_ANON = (
        '&foo' => sub { 'foo' }
        '$foo' => \$foo,
        ...
    );

=head2 %EXPORT_GEN

This is new to this module, L<Exporter> does not support it.

This allows you to export symbols that are generated on export. The key should
be the name of a symbol. The value should be a coderef that produces a
reference that will be exported.

When the generators are called they will receive 2 arguments, the package the
symbol is being exported into, and the symbol being imported (name may or may
not include sigil for subs).

    our %EXPORT_GEN = (
        '&foo' => sub {
            my $from_package = shift;
            my ($into_package, $symbol_name) = @_;
            ...
            return sub { ... };
        },
        ...
    );

=head2 %EXPORT_MAGIC

This is new to this module. L<Exporter> does not support it.

This allows you to define custom actions to run AFTER an export has been
injected into the consumers namespace. This is a good place to enable parser
hooks like with L<Devel::Declare>. These will NOT be run if a consumer uses a
custom assignment callback.

    our %EXPORT_MAGIC = (
        foo => sub {
            my $from = shift;    # Should be the package doing the exporting
            my %args = @_;

            my $into      = $args{into};         # Package symbol was exported into
            my $orig_name = $args{orig_name};    # Original name of the export (in the exporter)
            my $new_name  = $args{new_name};     # Name the symbol was imported as
            my $ref       = $args{ref};          # The reference to the symbol

            ...; # whatever you want, return is ignored.
        },
    );

=head1 CLASS METHODS

=over 4

=item Importer->import($from)

=item Importer->import($from, $version)

=item Importer->import($from, @imports)

=item Importer->import($from, $from_version, @imports)

=item Importer->import($importer_version, $from, ...)

This is the magic behind C<use Importer ...>.

=item Importer->import_into($from, $into, @imports)

=item Importer->import_into($from, $level, @imports)

You can use this to import symbols from C<$from> into C<$into>. C<$into> may
either be a package name, or a caller level to get the name from.

=item Importer->unimport()

=item Importer->unimport(@sub_name)

This is the magic behind C<no Importer ...>.

=item Importer->unimport_from($from, @sub_names)

=item Importer->unimport_from($level, @sub_names)

This lets you remove imported symbols from C<$from>. C<$from> my be a package
name, or a caller level.

=item my $exports = Importer->get($from, @imports)

This returns hashref of C<< { $name => $ref } >> for all the specified imports.

C<$from> should be the package from which to get the exports.

=item my @export_refs = Importer->get_list($from, @imports)

This returns a list of references for each import specified. Only the export
references are returned, the names are not.

C<$from> should be the package from which to get the exports.

=item $export_ref = Importer->get_one($from, $import)

This returns a single reference to a single export. If you provide multiple
imports then only the LAST one will be used.

C<$from> should be the package from which to get the exports.

=back

=head1 USING WITH OTHER EXPORTER IMPLEMENTATIONS

If you want your module to work with Importer, but you use something other than
L<Exporter> to define your exports, you can make it work be defining the
C<IMPORTER_MENU> method in your package. As well other exporters can be updated
to support Importer by putting this sub in your package.
B<IMPORTER_MENU() must be defined in your package, not a base class!>

    sub IMPORTER_MENU {
        my $class = shift;
        my ($into, $caller) = @_;

        return (
            export       => \@EXPORT,          # Default exports
            export_ok    => \@EXPORT_OK,       # Other allowed exports
            export_tags  => \%EXPORT_TAGS,     # Define tags
            export_fail  => \@EXPORT_FAIL,     # For subs that may not always be available
            export_anon  => \%EXPORT_ANON,     # Anonymous symbols to export
            export_magic => \%EXPORT_MAGIC,    # Magic to apply after a symbol is exported

            generate   => \&GENERATE,          # Sub to generate dynamic exports
                                               # OR
            export_gen => \%EXPORT_GEN,        # Hash of builders, key is symbol
                                               # name, value is sub that generates
                                               # the symbol ref.
        );
    }

    sub GENERATE {
        my ($symbol) = @_;

        ...

        return $ref;
    }

All exports must be listed in either C<@EXPORT> or C<@EXPORT_OK>, or be keys in
C<%EXPORT_GEN> or C<%EXPORT_ANON> to be allowed. 'export_tags', 'export_fail',
'export_anon', 'export_gen', and 'generate' are optional. You cannot combine
'generate' and 'export_gen'.

B<Note:> If your GENERATE sub needs the C<$class>, C<$into>, or C<$caller> then
your C<IMPORTER_MENU()> method will need to build an anonymous sub that closes
over them:

    sub IMPORTER_MENU {
        my $class = shift;
        my ($into, $caller) = @_;

        return (
            ...
            generate => sub { $class->GENERATE($into, $caller, @_) },
        );
    }

=head1 OO Interface

    use Importer;

    my $imp = Importer->new(from => 'Some::Exporter');

    $imp->do_import('Destination::Package');
    $imp->do_import('Another::Destination', @symbols);

Or, maybe more useful:

    my $imp = Importer->new(from => 'Carp');
    my $croak = $imp->get_one('croak');
    $croak->("This will croak");

=head2 OBJECT CONSTRUCTION

=over 4

=item $imp = Importer->new(from => 'Some::Exporter')

=item $imp = Importer->new(from => 'Some::Exporter', caller => [$package, $file, $line])

This is how you create a new Importer instance. C<< from => 'Some::Exporter' >>
is the only required argument. You may also specify the C<< caller => [...] >>
arrayref, which will be used only for error reporting. If you do not specify a
caller then Importer will attempt to find the caller dynamically every time it
needs it (this is slow and expensive, but necessary if you intend to re-use the
object.)

=back

=head2 OBJECT METHODS

=over 4

=item $imp->do_import($into)

=item $imp->do_import($into, @symbols)

This will import from the objects C<from> package into the C<$into> package.
You can provide a list of C<@symbols>, or you can leave it empty for the
defaults.

=item $imp->do_unimport()

=item $imp->do_unimport(@symbols)

This will remove imported symbols from the objects C<from> package. If you
specify a list of C<@symbols> then only the specified symbols will be removed,
otherwise all symbols imported using Importer will be removed.

B<Note:> Please be aware of the difference between C<do_import()> and
C<do_unimport()>. For import 'from' us used as the origin, in unimport it is
used as the target. This means you cannot re-use an instance to import and then
unimport.

=item ($into, $versions, $exclude, $symbols, $set) = $imp->parse_args('Dest::Package')

=item ($into, $versions, $exclude, $symbols, $set) = $imp->parse_args('Dest::Package', @symbols)

This parses arguments. The first argument must be the destination package.
Other arguments can be a mix of symbol names, tags, patterns, version numbers,
and exclusions.

=item $caller_ref = $imp->get_caller()

This will find the caller. This is mainly used for error reporting. IF the
object was constructed with a caller then that is what is returned, otherwise
this will scan the stack looking for the first call that does not originate
from a package that ISA Importer.

=item $imp->carp($warning)

Warn at the callers level.

=item $imp->croak($exception)

Die at the callers level.

=item $from_package = $imp->from()

Get the C<from> package that was specified at construction.

=item $file = $imp->from_file()

Get the filename for the C<from> package.

=item $imp->load_from()

This will load the C<from> package if it has not been loaded already. This uses
some magic to ensure errors in the load process are reported to the C<caller>.

=item $menu_hr = $imp->menu($into)

Get the export menu built from, or provided by the C<from> package. This is
cached after the first time it is called. Use C<< $imp->reload_menu() >> to
refresh it.

The menu structure looks like this:

    $menu = {
        # every valid export has a key in the lookup hashref, value is always
        # 1, key always includes the sigil
        lookup => {'&symbol_a' => 1, '$symbol_b' => 1, ...},

        # most exports are listed here, symbol name with sigil is key, value is
        # a reference to the symbol. If a symbol is missing it may be generated.
        exports => {'&symbol_a' => \&symbol_a, '$symbol_b' => \$symbol_b, ...},

        # Hashref of tags, tag name (without ':' prefix) is key, value is an
        # arrayref of symbol names, subs may have a sigil, but are not required
        # to.
        tags => { DEFAULT => [...], foo => [...], ... },

        # Magic to apply
        magic => { foo => sub { ... }, ... },

        # This is a hashref just like 'lookup'. Keys are symbols which may not
        # always be available. If there are no symbols in this category then
        # the value of the 'fail' key will be undef instead of a hashref.
        fail => { '&iffy_symbol' => 1, '\&only_on_linux' => 1 },
        # OR fail => undef,

        # If present, this subroutine knows how to generate references for the
        # symbols listed in 'lookup', but missing from 'exports'. References
        # this returns are NEVER cached.
        generate => sub { my $sym_name = shift; ...; return $symbol_ref },
    };

=item $imp->reload_menu($into)

This will reload the export menu from the C<from> package.

=item my $exports = $imp->get(@imports)

This returns hashref of C<< { $name => $ref } >> for all the specified imports.

=item my @export_refs = $imp->get_list(@imports)

This returns a list of references for each import specified. Only the export
references are returned, the names are not.

=item $export_ref = $imp->get_one($import)

This returns a single reference to a single export. If you provide multiple
imports then only the LAST one will be used.

=back

=head1 FUNCTIONS

These can be imported:

    use Importer 'Importer' => qw/import optimal_import/;

=over 4

=item $bool = optimal_import($from, $into, \@caller, @imports)

This function will attempt to import C<@imports> from the C<$from> package into
the C<$into> package. C<@caller> needs to have a package name, filename, and
line number. If this function fails then no exporting will actually happen.

If the import is successful this will return true.

If the import is unsuccessful this will return false, and no modifications to
the symbol table will occur.

=item $class->import(@imports)

If you write class intended to be used with L<Importer>, but also need to
provide a legacy C<import()> method for direct consumers of your class, you can
import this C<import()> method.

    package My::Exporter;

    # This will give you 'import()' much like 'use base "Exporter";'
    use Importer 'Importer' => qw/import/;

    ...

=back

=head1 SOURCE

The source code repository for Importer can be found at
L<http://github.com/exodist/Importer>.

=head1 MAINTAINERS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 AUTHORS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 COPYRIGHT

Copyright 2015 Chad Granum E<lt>exodist7@gmail.comE<gt>.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See L<http://dev.perl.org/licenses/>

=cut
