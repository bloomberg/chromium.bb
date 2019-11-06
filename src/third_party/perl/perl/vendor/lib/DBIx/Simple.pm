use 5.006;
use strict;
use DBI;
use Carp ();

$DBIx::Simple::VERSION = '1.35';
$Carp::Internal{$_} = 1
    for qw(DBIx::Simple DBIx::Simple::Result DBIx::Simple::DeadObject);

my $no_raiseerror = $ENV{PERL_DBIX_SIMPLE_NO_RAISEERROR};

my $quoted         = qr/(?:'[^']*'|"[^"]*")*/;  # 'foo''bar' simply matches the (?:) twice
my $quoted_mysql   = qr/(?:(?:[^\\']*(?:\\.[^\\']*)*)'|"(?:[^\\"]*(?:\\.[^\\"]*)*)")*/;

my %statements;       # "$db" => { "$st" => $st, ... }
my %old_statements;   # "$db" => [ [ $query, $st ], ... ]
my %keep_statements;  # "$db" => $int

my $err_message = '%s no longer usable (because of %%s)';
my $err_cause   = '%s at %s line %d';

package DBIx::Simple;

### private helper subs

sub _dummy { bless \my $dummy, 'DBIx::Simple::Dummy' }
sub _swap {
    my ($hash1, $hash2) = @_;
    my $tempref = ref $hash1;
    my $temphash = { %$hash1 };
    %$hash1 = %$hash2;
    bless $hash1, ref $hash2;
    %$hash2 = %$temphash;
    bless $hash2, $tempref;
}

### constructor

sub connect {
    my ($class, @arguments) = @_;
    my $self = { lc_columns => 1, result_class => 'DBIx::Simple::Result' };
    if (defined $arguments[0] and UNIVERSAL::isa($arguments[0], 'DBI::db')) {
        $self->{dont_disconnect} = 1;
	$self->{dbh} = shift @arguments;
	Carp::carp("Additional arguments for $class->connect are ignored")
	    if @arguments;
    } else {
	$arguments[3]->{PrintError} = 0
	    unless defined $arguments[3] and exists $arguments[3]{PrintError};
        $arguments[3]->{RaiseError} = 1
            unless $no_raiseerror
            or defined $arguments[3] and exists $arguments[3]{RaiseError};
	$self->{dbh} = DBI->connect(@arguments);
    }

    return undef unless $self->{dbh};

    $self->{dbd} = $self->{dbh}->{Driver}->{Name};
    bless $self, $class;

    $statements{$self}      = {};
    $old_statements{$self}  = [];
    $keep_statements{$self} = 16;

    return $self;
}

sub new {
    my ($class) = shift;
    $class->connect(@_);
}

### properties

sub keep_statements : lvalue { $keep_statements{ $_[0] } }
sub lc_columns      : lvalue { $_[0]->{lc_columns} }
sub result_class    : lvalue { $_[0]->{result_class} }

sub abstract : lvalue {
    require SQL::Abstract;
    $_[0]->{abstract} ||= SQL::Abstract->new;
}

sub error {
    my ($self) = @_;
    return 'DBI error: ' . (ref $self ? $self->{dbh}->errstr : $DBI::errstr);
}

sub dbh { $_[0]->{dbh} }

### private methods

# Replace (??) with (?, ?, ?, ...)
sub _replace_omniholder {
    my ($self, $query, $binds) = @_;
    return if $$query !~ /\(\?\?\)/;
    my $omniholders = 0;
    my $q = $self->{dbd} =~ /mysql/ ? $quoted_mysql : $quoted;
    $$query =~ s[($q|\(\?\?\))] {
        $1 eq '(??)'
        ? do {
            Carp::croak('There can be only one omniholder')
                if $omniholders++;
            '(' . join(', ', ('?') x @$binds) . ')'
        }
        : $1
    }eg;
}

# Invalidate and clean up
sub _die {
    my ($self, $cause) = @_;

    defined and $_->_die($cause, 0)
        for values %{ $statements{$self} },
        map $$_[1], @{ $old_statements{$self} };
    delete $statements{$self};
    delete $old_statements{$self};
    delete $keep_statements{$self};

    unless ($self->{dont_disconnect}) {
        # Conditional, because destruction order is not guaranteed
        # during global destruction.
        $self->{dbh}->disconnect() if defined $self->{dbh};
    }

    _swap(
        $self,
        bless {
            what  => 'Database object',
            cause => $cause
        }, 'DBIx::Simple::DeadObject'
    ) unless $cause =~ /DESTROY/;  # Let's not cause infinite loops :)
}

### public methods

sub query {
    my ($self, $query, @binds) = @_;
    $self->{success} = 0;

    $self->_replace_omniholder(\$query, \@binds);

    my $st;
    my $sth;

    my $old = $old_statements{$self};

    if (defined( my $i = (grep $old->[$_][0] eq $query, 0..$#$old)[0] )) {
        $st = splice(@$old, $i, 1)->[1];
        $sth = $st->{sth};
    } else {
        eval { $sth = $self->{dbh}->prepare($query) } or do {
            if ($@) {
                $@ =~ s/ at \S+ line \d+\.\n\z//;
                Carp::croak($@);
            }
            $self->{reason} = "Prepare failed ($DBI::errstr)";
            return _dummy;
        };

        # $self is quoted on purpose, to pass along the stringified version,
        # and avoid increasing reference count.
        $st = bless {
            db    => "$self",
            sth   => $sth,
            query => $query
        }, 'DBIx::Simple::Statement';
        $statements{$self}{$st} = $st;
    }

    eval { $sth->execute(@binds) } or do {
        if ($@) {
            $@ =~ s/ at \S+ line \d+\.\n\z//;
            Carp::croak($@);
        }

        $self->{reason} = "Execute failed ($DBI::errstr)";
	return _dummy;
    };

    $self->{success} = 1;

    return bless { st => $st, lc_columns => $self->{lc_columns} }, $self->{result_class};
}

sub begin_work     { $_[0]->{dbh}->begin_work }
sub begin          { $_[0]->begin_work        }
sub commit         { $_[0]->{dbh}->commit     }
sub rollback       { $_[0]->{dbh}->rollback   }
sub func           { shift->{dbh}->func(@_)   }

sub last_insert_id {
    my ($self) = @_;

    ($self->{dbi_version} ||= DBI->VERSION) >= 1.38 or Carp::croak(
    	"DBI v1.38 required for last_insert_id" .
	"--this is only $self->{dbi_version}, stopped"
    );

    return shift->{dbh}->last_insert_id(@_);
}

sub disconnect {
    my ($self) = @_;
    $self->_die(sprintf($err_cause, "$self->disconnect", (caller)[1, 2]));
    return 1;
}

sub DESTROY {
    my ($self) = @_;
    $self->_die(sprintf($err_cause, "$self->DESTROY", (caller)[1, 2]));
}

### public methods wrapping SQL::Abstract

for my $method (qw/select insert update delete/) {
    no strict 'refs';
    *$method = sub {
        my $self = shift;
        return $self->query($self->abstract->$method(@_));
    }
}

### public method wrapping SQL::Interp

sub iquery {
    require SQL::Interp;
    my $self = shift;
    return $self->query( SQL::Interp::sql_interp(@_) );
}

package DBIx::Simple::Dummy;

use overload
    '""' => sub { shift },
    bool => sub { 0 };

sub new      { bless \my $dummy, shift }
sub AUTOLOAD { return }

package DBIx::Simple::DeadObject;

sub _die {
    my ($self) = @_;
    Carp::croak(
        sprintf(
            "(This should NEVER happen!) " .
            sprintf($err_message, $self->{what}),
            $self->{cause}
        )
    );
}

sub AUTOLOAD {
    my ($self) = @_;
    Carp::croak(
        sprintf(
            sprintf($err_message, $self->{what}),
            $self->{cause}
        )
    );
}
sub DESTROY { }

package DBIx::Simple::Statement;

sub _die {
    my ($self, $cause, $save) = @_;

    $self->{sth}->finish() if defined $self->{sth};
    $self->{dead} = 1;

    my $stringy_db = "$self->{db}";
    my $stringy_self = "$self";

    my $foo = bless {
        what  => 'Statement object',
        cause => $cause
    }, 'DBIx::Simple::DeadObject';

    DBIx::Simple::_swap($self, $foo);

    my $old = $old_statements{ $foo->{db} };
    my $keep = $keep_statements{ $foo->{db} };

    if ($save and $keep) {
        $foo->{dead} = 0;
        shift @$old until @$old + 1 <= $keep;
        push @$old, [ $foo->{query}, $foo ];
    }

    delete $statements{ $stringy_db }{ $stringy_self };
}

sub DESTROY {
    # This better only happen during global destruction...
    return if $_[0]->{dead};
    $_[0]->_die('Ehm', 0);
}

package DBIx::Simple::Result;

sub _die {
    my ($self, $cause) = @_;
    if ($cause) {
        $self->{st}->_die($cause, 1);
        DBIx::Simple::_swap(
            $self,
            bless {
                what  => 'Result object',
                cause => $cause,
            }, 'DBIx::Simple::DeadObject'
        );
    } else {
        $cause = $self->{st}->{cause};
        DBIx::Simple::_swap(
            $self,
            bless {
                what  => 'Result object',
                cause => $cause
            }, 'DBIx::Simple::DeadObject'
        );
        Carp::croak(
            sprintf(
                sprintf($err_message, $self->{what}),
                $cause
            )
        );
    }
}

sub func { shift->{st}->{sth}->func(@_) }
sub attr { my $dummy = $_[0]->{st}->{sth}->{$_[1]} }

sub columns {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    my $c = $_[0]->{st}->{sth}->{ $_[0]->{lc_columns} ? 'NAME_lc' : 'NAME' };
    return wantarray ? @$c : $c;
}

sub bind {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    $_[0]->{st}->{sth}->bind_columns(\@_[1..$#_]);
}


### Single

sub fetch {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    return $_[0]->{st}->{sth}->fetch;
}

sub into {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    my $sth = $_[0]->{st}->{sth};
    $sth->bind_columns(\@_[1..$#_]) if @_ > 1;
    return $sth->fetch;
}

sub list {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    return $_[0]->{st}->{sth}->fetchrow_array if wantarray;
    return($_[0]->{st}->{sth}->fetchrow_array)[-1];
}

sub array {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    my $row = $_[0]->{st}->{sth}->fetchrow_arrayref or return;
    return [ @$row ];
}

sub hash {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    return $_[0]->{st}->{sth}->fetchrow_hashref(
        $_[0]->{lc_columns} ? 'NAME_lc' : 'NAME'
    );
}

sub kv_list {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    my @keys   = $_[0]->columns;
    my $values = $_[0]->array or return;
    Carp::croak("Different numbers of column names and values")
        if @keys != @$values;
    return   map { $keys[$_], $values->[$_] } 0 .. $#keys   if wantarray;
    return [ map { $keys[$_], $values->[$_] } 0 .. $#keys ];
}

sub kv_array {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    scalar shift->kv_list(@_);
}

sub object {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    my $self = shift;
    my $class = shift || ':RowObject';
    if ($class =~ /^:/) {
        $class = "DBIx::Simple::Result:$class";
        (my $package = "$class.pm") =~ s[::][/]g;
        require $package;
    }
    if ($class->can('new_from_dbix_simple')) {
        return scalar $class->new_from_dbix_simple($self, @_);
    }
    if ($class->can('new')) {
        return $class->new( $self->kv_list );
    }
    Carp::croak(
        qq(Can't locate object method "new_from_dbix_simple" or "new" ) .
        qq(via package "$class" (perhaps you forgot to load "$class"?))
    );
}

### Slurp

sub flat {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    return   map @$_, $_[0]->arrays if wantarray;
    return [ map @$_, $_[0]->arrays ];
}

sub arrays {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    return @{ $_[0]->{st}->{sth}->fetchall_arrayref } if wantarray;
    return    $_[0]->{st}->{sth}->fetchall_arrayref;
}

sub hashes {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    my @return;
    my $dummy;
    push @return, $dummy while $dummy = $_[0]->hash;
    return wantarray ? @return : \@return;
}

sub kv_flat {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    return   map @$_, $_[0]->kv_arrays if wantarray;
    return [ map @$_, $_[0]->kv_arrays ];
}

sub kv_arrays {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    my @return;
    my $dummy;
    push @return, $dummy while $dummy = $_[0]->kv_array;
    return wantarray ? @return : \@return;
}

sub objects {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    my $self = shift;
    my $class = shift || ':RowObject';
    if ($class =~ /^:/) {
        $class = "DBIx::Simple::Result:$class";
        (my $package = "$class.pm") =~ s[::][/]g;
        require $package;
    }
    if ($class->can('new_from_dbix_simple')) {
        return   $class->new_from_dbix_simple($self, @_) if wantarray;
        return [ $class->new_from_dbix_simple($self, @_) ];
    }
    if ($class->can('new')) {
        return   map { $class->new( @$_ ) } $self->kv_arrays if wantarray;
        return [ map { $class->new( @$_ ) } $self->kv_arrays ];
    }
    Carp::croak(
        qq(Can't locate object method "new_from_dbix_simple" or "new" ) .
        qq(via package "$class" (perhaps you forgot to load "$class"?))
    );
}

sub map_hashes {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    my ($self, $keyname) = @_;
    Carp::croak('Key column name not optional') if not defined $keyname;
    my @rows = $self->hashes;
    my @keys;
    push @keys, delete $_->{$keyname} for @rows;
    my %return;
    @return{@keys} = @rows;
    return wantarray ? %return : \%return;
}

sub map_arrays {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    my ($self, $keyindex) = @_;
    $keyindex += 0;
    my @rows = $self->arrays;
    my @keys;
    push @keys, splice @$_, $keyindex, 1 for @rows;
    my %return;
    @return{@keys} = @rows;
    return wantarray ? %return : \%return;
}

sub map {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    return   map @$_, @{ $_[0]->{st}->{sth}->fetchall_arrayref } if wantarray;
    return { map @$_, @{ $_[0]->{st}->{sth}->fetchall_arrayref } };
}

sub rows {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    $_[0]->{st}->{sth}->rows;
}

sub xto {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    require DBIx::XHTML_Table;
    my $self = shift;
    my $attr = ref $_[0] ? $_[0] : { @_ };

    # Old DBD::SQLite (.29) spits out garbage if done *after* fetching.
    my $columns = $self->{st}->{sth}->{NAME};

    return DBIx::XHTML_Table->new(
        scalar $self->arrays,
        $columns,
        $attr
    );
}

sub html {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    my $self = shift;
    my $attr = ref $_[0] ? $_[0] : { @_ };
    return $self->xto($attr)->output($attr);
}

sub text {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    my ($self, $type) = @_;
    my $text_table = defined $type && length $type
        ? 0
        : eval { require Text::Table; $type = 'table'; 1 };
    $type ||= 'neat';
    if ($type eq 'box' or $type eq 'table') {
        my $box = $type eq 'box';
        $text_table or require Text::Table;
        my @columns = map +{ title => $_, align_title => 'center' },
            @{ $self->{st}->{sth}->{NAME} };
        my $c = 0;
        splice @columns, $_ + $c++, 0, \' | ' for 1 .. $#columns;
        my $table = Text::Table->new(
            ($box ? \'| ' : ()),
            @columns,
            ($box ? \' |' : ())
        );
        $table->load($self->arrays);
        my $rule = $table->rule(qw/- +/);
        return join '',
            ($box ? $rule : ()),
            $table->title, $rule, $table->body,
            ($box ? $rule : ());
    }
    Carp::carp("Unknown type '$type'; using 'neat'") if $type ne 'neat';
    return join '', map DBI::neat_list($_) . "\n", $self->arrays;
}

sub finish {
    $_[0]->_die if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    my ($self) = @_;
    $self->_die(
        sprintf($err_cause, "$self->finish", (caller)[1, 2])
    );
}

sub DESTROY {
    return if ref $_[0]->{st} eq 'DBIx::Simple::DeadObject';
    my ($self) = @_;
    $self->_die(
        sprintf($err_cause, "$self->DESTROY", (caller)[1, 2])
    );
}

1;

__END__

=head1 NAME

DBIx::Simple - Very complete easy-to-use OO interface to DBI

=head1 SYNOPSIS

=head2 DBIx::Simple

    $db = DBIx::Simple->connect(...)  # or ->new

    $db->keep_statements = 16
    $db->lc_columns = 1
    $db->result_class = 'DBIx::Simple::Result';

    $db->begin_work         $db->commit
    $db->rollback           $db->disconnect
    $db->func(...)          $db->last_insert_id

    $result = $db->query(...)

=head2 DBIx::SImple + SQL::Interp

    $result = $db->iquery(...)

=head2 DBIx::Simple + SQL::Abstract

    $db->abstract = SQL::Abstract->new(...)

    $result = $db->select(...)
    $result = $db->insert(...)
    $result = $db->update(...)
    $result = $db->delete(...)

=head2 DBIx::Simple::Result

    @columns = $result->columns

    $result->into($foo, $bar, $baz)
    $row = $result->fetch

    @row = $result->list      @rows = $result->flat
    $row = $result->array     @rows = $result->arrays
    $row = $result->hash      @rows = $result->hashes
    @row = $result->kv_list   @rows = $result->kv_flat
    $row = $result->kv_array  @rows = $result->kv_arrays
    $obj = $result->object    @objs = $result->objects

    %map = $result->map_arrays(...)
    %map = $result->map_hashes(...)
    %map = $result->map

    $rows = $result->rows

    $dump = $result->text

    $result->finish

=head2 DBIx::Simple::Result + DBIx::XHTML_Table

    $html = $result->html(...)

    $table_object = $result->xto(...)

=head2 Examples

Please read L<DBIx::Simple::Examples> for code examples.

=head1 DESCRIPTION

DBIx::Simple provides a simplified interface to DBI, Perl's powerful database
module.

This module is aimed at rapid development and easy maintenance. Query
preparation and execution are combined in a single method, the result object
(which is a wrapper around the statement handle) provides easy row-by-row and
slurping methods.

The C<query> method returns either a result object, or a dummy object. The
dummy object returns undef (or an empty list) for all methods and when used in
boolean context, is false. The dummy object lets you postpone (or skip) error
checking, but it also makes immediate error checking simply C<<
$db->query(...) or die $db->error >>.

=head2 DBIx::Simple methods

=head3 Class methods

=over 14

=item C<connect($dbh)>, C<connect($dsn, $user, $pass, \%options)>

=item C<new($dbh)>, C<new($dsn, $user, $pass, \%options)>

The C<connect> or C<new> class method takes either an existing DBI object
($dbh), or a list of arguments to pass to C<< DBI->connect >>. See L<DBI> for a
detailed description.

You cannot use this method to clone a DBIx::Simple object: the $dbh passed
should be a DBI::db object, not a DBIx::Simple object.

For new connections, PrintError is disabled by default. If you enable it,
beware that it will report line numbers in DBIx/Simple.pm.

For new connections, B<RaiseError is enabled by default> unless the environment
variable C<PERL_DBIX_SIMPLE_NO_RAISEERROR> is set to a non-empty non-0 value.

This method is the constructor and returns a DBIx::Simple object on success. On
failure, it returns undef.

=back

=head3 Object methods

=over 14

=item C<query($query, @values)>

Prepares and executes the query and returns a result object.

If the string C<(??)> is present in the query, it is replaced with a list of as
many question marks as @values.

The database drivers substitute placeholders (question marks that do not appear
in quoted literals) in the query with the given @values, after them escaping
them. You should always use placeholders, and never use raw user input in
database queries.

On success, returns a DBIx::Simple::Result object. On failure, returns a
DBIx::Simple::Dummy object.

=item C<iquery(...)>

Uses SQL::Interp to interpolate values into a query, and uses the resulting
generated query and bind arguments with C<query>. See SQL::Interp's
documentation for usage information.

Requires Mark Storberg's SQL::Interp, which is available from CPAN. SQL::Interp
is a fork from David Manura's SQL::Interpolate.

=item C<select>, C<insert>, C<update>, C<delete>

Calls the respective method on C<abstract>, and uses the resulting generated
query and bind arguments with C<query>. See SQL::Abstract's documentation for
usage information. You can override the object by assigning to the C<abstract>
property.

Requires Nathan Wiger's SQL::Abstract, which is available from CPAN.

=item C<begin_work>, C<begin>, C<commit>, C<rollback>

These transaction related methods call the DBI respective methods and
Do What You Mean. See L<DBI> for details.

C<begin> is an alias for C<begin_work>.

=item C<func(...)>

Calls the C<func> method of DBI. See L<DBI> for details.

=item C<last_insert_id(...)>

Calls the C<last_insert_id> method of DBI. See L<DBI> for details. Note that
this feature requires DBI 1.38 or newer.

=item C<disconnect>

Destroys (finishes) active statements and disconnects. Whenever the database
object is destroyed, this happens automatically if DBIx::Simple handled the
connection (i.e. you didn't use an existing DBI handle). After disconnecting,
you can no longer use the database object or any of its result objects.

=back

=head3 Object properties

=over 14

=item C<dbh>

Exposes the internal database handle. Use this only if you know what you are
doing. Keeping a reference or doing queries can interfere with DBIx::Simple's
garbage collection and error reporting.

=item C<lc_columns = $bool>

When true at time of query execution, makes several result object methods use
lower cased column names. C<lc_columns> is true by default.

=item C<keep_statements = $integer>

Sets the number of statement objects that DBIx::Simple can keep for reuse. This
can dramatically speed up repeated queries (like when used in a loop).
C<keep_statements> is 16 by default.

A query is only reused if it equals a previously used one literally. This means
that to benefit from this caching mechanism, you must use placeholders and
never interpolate variables yourself.

    # Wrong:
    $db->query("INSERT INTO foo VALUES ('$foo', '$bar', '$baz')");
    $db->query("SELECT FROM foo WHERE foo = '$foo' OR bar = '$bar'");

    # Right:
    $db->query('INSERT INTO foo VALUES (??)', $foo, $bar, $baz);
    $db->query('SELECT FROM foo WHERE foo = ? OR bar = ?', $foo, $baz);

Of course, automatic value escaping is a much better reason for using
placeholders.

=item C<result_class = $string>

Class to use for result objects. Defaults to DBIx::Simple::Result. A
constructor is not used.

=item C<error>

Returns the error string of the last DBI method. See the discussion of "C<err>"
and "C<errstr>" in L<DBI>.

=item C<< abstract = SQL::Abstract->new(...) >>

Sets the object to use with the C<select>, C<insert>, C<update> and C<delete>
methods. On first access, will create one with SQL::Abstract's default options.

Requires Nathan Wiger's SQL::Abstract, which is available from CPAN.

In theory, you can assign any object to this property, as long as that object
has these four methods, and they return a list suitable for use with the
C<query> method.

=back

=head2 DBIx::Simple::Dummy

The C<query> method of DBIx::Simple returns a dummy object on failure. Its
methods all return an empty list or undef, depending on context. When used in
boolean context, a dummy object evaluates to false.

=head2 DBIx::Simple::Result methods

Methods documented to return "a list" return a reference to an array of the
same in scalar context, unless something else is explicitly mentioned.

=over 14

=item C<columns>

Returns a list of column names. Affected by C<lc_columns>.

=item C<bind(LIST)>

Binds the given LIST of variables to the columns. Unlike with DBI's
C<bind_columns>, passing references is not needed.

Bound variables are very efficient. Binding a tied variable doesn't work.

=item C<attr(...)>

Returns a copy of an sth attribute (property). See L<DBI/"Statement Handle
Attributes"> for details.

=item C<func(...)>

This calls the C<func> method on the sth of DBI. See L<DBI> for details.

=item C<rows>

Returns the number of rows affected by the last row affecting command, or -1 if
the number of rows is not known or not available.

For SELECT statements, it is generally not possible to know how many rows are
returned. MySQL does provide this information. See L<DBI> for a detailed
explanation.

=item C<finish>

Finishes the statement. After finishing a statement, it can no longer be used.
When the result object is destroyed, its statement handle is automatically
finished and destroyed. There should be no reason to call this method
explicitly; just let the result object go out of scope.

=back

=head3 Fetching a single row at a time

=over 14

=item C<fetch>

Returns a reference to the array that holds the values. This is the same array
every time.

Subsequent fetches (using any method) may change the values in the variables
passed and the returned reference's array.

=item C<into(LIST)>

Combines C<bind> with C<fetch>. Returns what C<fetch> returns.

=item C<list>

Returns a list of values, or (in scalar context), only the last value.

=item C<array>

Returns a reference to an array.

=item C<hash>

Returns a reference to a hash, keyed by column name. Affected by C<lc_columns>.

=item C<kv_list>

Returns an ordered list of interleaved keys and values. Affected by
C<lc_columns>.

=item C<kv_array>

Returns a reference to an array of interleaved column names and values. Like
kv, but returns an array reference even in list context. Affected by
C<lc_columns>.

=item C<object($class, ...)>

Returns an instance of $class. See "Object construction". Possibly affected by
C<lc_columns>.

=back

=head3 Fetching all remaining rows

=over 14

=item C<flat>

Returns a flattened list.

=item C<arrays>

Returns a list of references to arrays

=item C<hashes>

Returns a list of references to hashes, keyed by column name. Affected by
C<lc_columns>.

=item C<kv_flat>

Returns an flattened list of interleaved column names and values. Affected by
C<lc_columns>.

=item C<kv_arrays>

Returns a list of references to arrays of interleaved column names and values.
Affected by C<lc_columns>.

=item C<objects($class, ...)>

Returns a list of instances of $class. See "Object construction". Possibly
affected by C<lc_columns>.

=item C<map_arrays($column_number)>

Constructs a hash of array references keyed by the values in the chosen column,
and returns a list of interleaved keys and values, or (in scalar context), a
reference to a hash.

=item C<map_hashes($column_name)>

Constructs a hash of hash references keyed by the values in the chosen column,
and returns a list of interleaved keys and values, or (in scalar context), a
reference to a hash. Affected by C<lc_columns>.

=item C<map>

Constructs a simple hash, using the two columns as key/value pairs. Should
only be used with queries that return two columns. Returns a list of interleaved
keys and values, or (in scalar context), a reference to a hash.

=item C<xto(%attr)>

Returns a DBIx::XHTML_Table object, passing the constructor a reference to
C<%attr>.

Requires Jeffrey Hayes Anderson's DBIx::XHTML_Table, which is available from
CPAN.

In general, using the C<html> method (described below) is much easier. C<xto>
is available in case you need more flexibility. Not affected by C<lc_columns>.

=item C<html(%attr)>

Returns an (X)HTML formatted table, using the DBIx::XHTML_Table module. Passes
a reference to C<%attr> to both the constructor and the C<output> method.

Requires Jeffrey Hayes Anderson's DBIx::XHTML_Table, which is available from
CPAN.

This method is a shortcut method. That means that

    $result->html

    $result->html(
        tr => { bgcolor => [ 'silver', 'white' ] },
        no_ucfirst => 1
    )

do the same as:

    $result->xto->output

    $result->xto(
        tr => { bgcolor => [ 'silver', 'white' ] }
    )->output(
        no_ucfirst => 1
    );

=item C<text($type)>

Returns a string with a simple text representation of the data. C<$type>
can be any of: C<neat>, C<table>, C<box>. It defaults to C<table> if
Text::Table is installed, to C<neat> if it isn't.

C<table> and C<box> require Anno Siegel's Text::Table, which is available from
CPAN.

=back

=head2 Object construction

DBIx::Simple has basic support for returning results as objects. The actual
construction method has to be provided by the chosen class, making this
functionality rather advanced and perhaps unsuited for beginning programmers.

When the C<object> or C<objects> method is called on the result object returned
by one of the query methods, two approaches are tried. In either case, pass the
name of a class as the first argument. A prefix of a single colon can be used
as an alias for C<DBIx::Simple::Result::>, e.g. C<":Example"> is short for
C<"DBIx::Simple::Result::Example">. When this shortcut is used, the
corresponding module is loaded automatically.

The default class when no class is given, is C<:RowObject>. It requires Jos
Boumans' Object::Accessor, which is available from CPAN.

=head3 Simple object construction

When C<object> is given a class that provides a C<new> method, but not a
C<new_from_dbix_simple> method, C<new> is called with a list of interleaved
column names and values, like a flattened hash, but ordered. C<objects> causes
C<new> to be called multiple times, once for each remaining row.

Example:

    {
        package DBIx::Simple::Result::ObjectExample;
        sub new {
            my ($class, %args) = @_;
            return bless $class, \%args;
        }

        sub foo { ... }
        sub bar { ... }
    }


    $db->query('SELECT foo, bar FROM baz')->object(':ObjectExample')->foo();

=head3 Advanced object construction

When C<object> or C<objects> is given a class that provides a
C<new_from_dbix_simple> method, any C<new> is ignored, and
C<new_from_dbix_simple> is called with a list of the DBIx::Simple::Result
object and any arguments passed to C<object> or C<objects>.

C<new_from_dbix_simple> is called in scalar context for C<object>, and in list
context for C<objects>. In scalar context, it should fetch I<exactly one row>,
and in list context, it should fetch I<all remaining rows>.

Example:

    {
        package DBIx::Simple::Result::ObjectExample;
        sub new_from_dbix_simple {
            my ($class, $result, @args) = @_;
            return map { bless $class, $_ } $result->hashes if wantarray;
            return       bless $class, $result->hash;
        }

        sub foo { ... }
        sub bar { ... }
    }

    $db->query('SELECT foo, bar FROM baz')->object(':ObjectExample')->foo();

=head1 MISCELLANEOUS

The mapping methods do not check whether the keys are unique. Rows that are
fetched later overwrite earlier ones.


=head1 LICENSE

Pick your favourite OSI approved license :)

http://www.opensource.org/licenses/alphabetical

=head1 AUTHOR

Juerd Waalboer <#####@juerd.nl> <http://juerd.nl/>

=head1 SEE ALSO

L<perl>, L<perlref>

L<DBI>, L<DBIx::Simple::Examples>, L<SQL::Abstract>, L<DBIx::XHTML_Table>

=cut

