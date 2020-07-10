package SQL::Parser;

######################################################################
#
# This module is copyright (c), 2001,2005 by Jeff Zucker.
# This module is copyright (c), 2007-2017 by Jens Rehsack.
# All rights reserved.
#
# It may be freely distributed under the same terms as Perl itself.
# See below for help and copyright information (search for SYNOPSIS).
#
######################################################################

use strict;
use warnings FATAL => "all";
use vars qw($VERSION);
use constant FUNCTION_NAMES => join( '|', qw(TRIM SUBSTRING) );
use constant BAREWORD_FUNCTIONS =>
  join( '|', qw(TRIM SUBSTRING CURRENT_DATE CURDATE CURRENT_TIME CURTIME CURRENT_TIMESTAMP NOW UNIX_TIMESTAMP PI DBNAME) );
use Carp qw(carp croak);
use Params::Util qw(_ARRAY0 _ARRAY _HASH);
use Scalar::Util qw(looks_like_number);
use Text::Balanced qw(extract_bracketed);

$VERSION = '1.412';

BEGIN
{
    if ( $ENV{SQL_USER_DEFS} ) { require SQL::UserDefs; }
}

#############################
# PUBLIC METHODS
#############################

sub new
{
    my $class = shift;
    my $dialect = shift || 'ANSI';
    $dialect = 'ANSI'    if ( uc $dialect eq 'ANSI' );
    $dialect = 'AnyData' if ( ( uc $dialect eq 'ANYDATA' ) or ( uc $dialect eq 'CSV' ) );
    $dialect = 'AnyData' if ( $dialect eq 'SQL::Eval' );

    my $flags = shift || {};
    $flags->{dialect} = $dialect;
    $flags->{PrintError} = 1 unless ( defined( $flags->{PrintError} ) );

    my $self = bless( $flags, $class );
    $self->dialect( $self->{dialect} );
    $self->set_feature_flags( $self->{select}, $self->{create} );

    $self->LOAD('LOAD SQL::Statement::Functions');

    return $self;
}

sub parse
{
    my ( $self, $sql ) = @_;
    $self->dialect( $self->{dialect} ) unless ( $self->{dialect_set} );
    $sql =~ s/^\s+//;
    $sql =~ s/\s+$//;
    $sql =~ s/\s*;$//;
    $self->{struct}                    = { dialect => $self->{dialect} };
    $self->{tmp}                       = {};
    $self->{original_string}           = $sql;
    $self->{struct}->{original_string} = $sql;

    ################################################################
    #
    # COMMENTS

    # C-STYLE
    #
    my $comment_re = $self->{comment_re} || '(\/\*.*?\*\/)';
    $self->{comment_re} = $comment_re;
    my $starts_with_comment;
    if ( $sql =~ /^\s*$comment_re(.*)$/s )
    {
        $self->{comment}     = $1;
        $sql                 = $2;
        $starts_with_comment = 1;
    }

    # SQL STYLE
    #
    # SQL-style comment can not begin inside quotes.
    if ( $sql =~ s/^([^']*?(?:'[^']*'[^'])*?)(--.*)(\n|$)/$1$3/ )
    {
        $self->{comment} = $2;
    }
    ################################################################

    $sql = $self->clean_sql($sql);
    my ($com) = $sql =~ m/^\s*(\S+)\s+/s;
    if ( !$com )
    {
        return 1 if ($starts_with_comment);
        return $self->do_err("Incomplete statement!");
    }
    $com                                    = uc $com;
    $self->{opts}->{valid_commands}->{CALL} = 1;
    $self->{opts}->{valid_commands}->{LOAD} = 1;
    if ( $self->{opts}->{valid_commands}->{$com} )
    {
        my $rv = $self->$com($sql);
        delete $self->{struct}->{literals};

        return $self->do_err("No command found!") unless ( $self->{struct}->{command} );

        $self->replace_quoted_ids();

        my @tables = @{ $self->{struct}->{table_names} }
          if ( defined( _ARRAY0( $self->{struct}->{table_names} ) ) );
        push( @{ $self->{struct}->{org_table_names} }, @tables );
        # REMOVE schema.table info if present
        @tables = map { s/^.*\.([^\.]+)$/$1/; ( -1 == index( $_, '"' ) ) ? lc $_ : $_ } @tables;

        if ( exists( $self->{struct}->{join} ) && !defined( _HASH( $self->{struct}->{join} ) ) )
        {
            delete $self->{struct}->{join};
        }
        else
        {
            $self->{struct}->{join}->{table_order} = $self->{struct}->{table_names}
              if ( defined( $self->{struct}->{join}->{table_order} )
                && !defined( _ARRAY0( $self->{struct}->{join}->{table_order} ) ) );
            @{ $self->{struct}->{join}->{keycols} } =
              map { lc $_ } @{ $self->{struct}->{join}->{keycols} }
              if ( $self->{struct}->{join}->{keycols} );
            @{ $self->{struct}->{join}->{shared_cols} } =
              map { lc $_ } @{ $self->{struct}->{join}->{shared_cols} }
              if ( $self->{struct}->{join}->{shared_cols} );
        }

        if (   defined( $self->{struct}->{column_defs} )
            && defined( _ARRAY( $self->{struct}->{column_defs} ) ) )
        {
            my $colname;
            # FIXME SUBSTR('*')
            my @fine_defs =
              grep { defined( $_->{fullorg} ) && ( -1 == index( $_->{fullorg}, '*' ) ) } @{ $self->{struct}->{column_defs} };
            foreach my $col (@fine_defs)
            {
                my $colname = $col->{fullorg};
                #$cn = lc $cn unless ( $cn =~ m/^(?:\w+\.)?"/ );
                push( @{ $self->{struct}->{org_col_names} }, $self->{struct}->{ORG_NAME}->{$colname} || $colname );
            }

            unless ( $com eq 'CREATE' )
            {
                $self->{struct}->{table_names} = \@tables;
                #  For RR aliases, added quoted id protection from upper casing
                foreach my $col (@fine_defs)
                {
                    # defined( $col->{fullorg} ) && ( -1 == index( $col->{fullorg}, '*' ) ) or next;
                    my $orgname = $colname = $col->{fullorg};
                    $colname =~ m/^(?:\p{Word}+\.)?"/ or $colname = lc $colname;
                    defined( $self->{struct}->{ORG_NAME}->{$colname} ) and next;
                    $self->{struct}->{ORG_NAME}->{$colname} =
                      $self->{struct}->{ORG_NAME}->{$orgname};
                }
                #my @uCols = map { ( $_ =~ /^(\w+\.)?"/ ) ? $_ : lc $_ } @{ $self->{struct}->{column_names} };
                #$self->{struct}->{column_names} = \@uCols;
            }
        }

        return $rv;
    }
    else
    {
        $self->{struct} = {};
        if ( $ENV{SQL_USER_DEFS} )
        {
            return SQL::UserDefs::user_parse( $self, $sql );
        }
        return $self->do_err("Command '$com' not recognized or not supported!");
    }
}

sub replace_quoted_commas
{
    my ( $self, $id ) = @_;
    $id =~ s/\?COMMA\?/,/gs;
    return $id;
}

sub replace_quoted_ids
{
    my ( $self, $id ) = @_;
    $self->{struct}->{quoted_ids} or $self->{struct}->{literals} or return;
    if ($id)
    {
        if ( $id =~ /^\?QI(\d+)\?$/ )
        {
            return '"' . $self->{struct}->{quoted_ids}->[$1] . '"';
        }
        elsif ( $id =~ /^\?(\d+)\?$/ )
        {
            return $self->{struct}->{literals}->[$1];
        }
        else
        {
            return $id;
        }
    }
    return unless defined $self->{struct}->{table_names};
    my @tables = @{ $self->{struct}->{table_names} };
    for my $t (@tables)
    {
        if ( $t =~ /^\?QI(.+)\?$/ )
        {
            $t = '"' . $self->{struct}->{quoted_ids}->[$1] . '"';
        }
	elsif( $t =~ /^\?(\d+)\?$/ )
	{
            $t = $self->{struct}->{literals}->[$1];
	}
    }
    $self->{struct}->{table_names} = \@tables;
    delete $self->{struct}->{quoted_ids};
}

sub structure { $_[0]->{struct} }
sub command { my $x = $_[0]->{struct}->{command} || '' }

sub feature
{
    my ( $self, $opt_class, $opt_name, $opt_value ) = @_;
    if ( defined $opt_value )
    {
        if ( $opt_class eq 'select' )
        {
            $self->set_feature_flags( { "join" => $opt_value } );
        }
        elsif ( $opt_class eq 'create' )
        {
            $self->set_feature_flags( undef, { $opt_name => $opt_value } );
        }
        else
        {

            # patch from chromatic
            $self->{opts}->{$opt_class}->{$opt_name} = $opt_value;

            # $self->{$opt_class}->{$opt_name} = $opt_value;
        }
    }
    else
    {
        return $self->{opts}->{$opt_class}->{$opt_name};
    }
}

sub errstr { $_[0]->{struct}->{errstr} }

sub list
{
    my $self = shift;
    my $com  = uc shift;
    return () if $com !~ /COMMANDS|RESERVED|TYPES|OPS|OPTIONS|DIALECTS/i;
    $com = 'valid_commands'             if $com eq 'COMMANDS';
    $com = 'valid_comparison_operators' if $com eq 'OPS';
    $com = 'valid_data_types'           if $com eq 'TYPES';
    $com = 'valid_options'              if $com eq 'OPTIONS';
    $com = 'reserved_words'             if $com eq 'RESERVED';
    $self->dialect( $self->{dialect} ) unless $self->{dialect_set};

    return sort keys %{ $self->{opts}->{$com} } unless $com eq 'DIALECTS';
    my $dDir = "SQL/Dialects";
    my @dialects;
    for my $dir (@INC)
    {
        local *D;

        if ( opendir( D, "$dir/$dDir" ) )
        {
            @dialects = grep /.*\.pm$/, readdir(D);
            last;
        }
    }
    @dialects = map { s/\.pm$//; $_ } @dialects;
    return @dialects;
}

sub dialect
{
    my ( $self, $dialect ) = @_;
    return $self->{dialect} unless ($dialect);
    return $self->{dialect} if ( $self->{dialect_set} );
    $self->{opts} = {};
    my $mod_class = "SQL::Dialects::$dialect";

    $self->_load_class($mod_class) unless $mod_class->can("get_config");

    # This is here for backwards compatibility with existing dialects
    # before the had the role to add new methods.
    $self->_inject_role( "SQL::Dialects::Role", $mod_class )
      unless ( $mod_class->can("get_config_as_hash") );

    $self->{opts} = $mod_class->get_config_as_hash();

    $self->create_op_regexen();
    $self->{dialect} = $dialect;
    $self->{dialect_set}++;

    return $self->{dialect};
}

sub _load_class
{
    my ( $self, $class ) = @_;

    my $mod = $class;
    $mod =~ s{::}{/}g;
    $mod .= ".pm";

    local ( $!, $@ );
    eval { require "$mod"; } or return $self->do_err($@);

    return 1;
}

sub _inject_role
{
    my ( $self, $role, $dest ) = @_;

    eval qq{
        package $dest;
        use $role;
        1;
    } or croak "Can't inject $role into $dest: $@";
}

sub create_op_regexen
{
    my ($self) = @_;

    #
    #	DAA precompute the predicate operator regex's
    #
    #       JZ moved this into a sub so it can be called from both
    #       dialect() and from CREATE_OPERATOR and DROP_OPERATOR
    #       since those also modify the available operators
    #
    my @allops = keys %{ $self->{opts}->{valid_comparison_operators} };

    #
    #	complement operators
    #
    my @notops;
    for (@allops)
    {
        push( @notops, $_ )
          if /NOT/i;
    }
    $self->{opts}->{valid_comparison_NOT_ops_regex} = '^\s*(.+)\s+(' . join( '|', @notops ) . ')\s+(.*)\s*$'
      if scalar @notops;

    #
    #	<>, <=, >= operators
    #
    my @compops;
    for (@allops)
    {
        push( @compops, $_ )
          if /<=|>=|<>/;
    }
    $self->{opts}->{valid_comparison_twochar_ops_regex} = '^\s*(.+)\s+(' . join( '|', @compops ) . ')\s+(.*)\s*$'
      if scalar @compops;

    #
    #	everything
    #
    $self->{opts}->{valid_comparison_ops_regex} = '^\s*(.+)\s+(' . join( '|', @allops ) . ')\s+(.*)\s*$'
      if scalar @allops;

    #
    #	end DAA
    #
}

##################################################################
# SQL COMMANDS
##################################################################

####################################################
# DROP TABLE <table_name>
####################################################
sub DROP
{
    my ( $self, $stmt ) = @_;
    my $features = 'TYPE|KEYWORD|FUNCTION|OPERATOR|PREDICATE';
    if ( $stmt =~ /^\s*DROP\s+($features)\s+(.+)$/si )
    {
        my ( $sub, $arg ) = ( $1, $2 );
        $sub = 'DROP_' . $sub;
        return $self->$sub($arg);
    }
    my $table_name;
    $self->{struct}->{command} = 'DROP';
    if ( $stmt =~ /^\s*DROP\s+TABLE\s+IF\s+EXISTS\s+(.*)$/si )
    {
        $stmt = "DROP TABLE $1";
        $self->{struct}->{ignore_missing_table} = 1;
    }
    if ( $stmt =~ /^\s*DROP\s+(\S+)\s+(.+)$/si )
    {
        my $com2 = $1 || '';
        $table_name = $2;
        if ( $com2 !~ /^TABLE$/i )
        {
            return $self->do_err("The command 'DROP $com2' is not recognized or not supported!");
        }
        $table_name =~ s/^\s+//;
        $table_name =~ s/\s+$//;
        if ( $table_name =~ /(\S+) (RESTRICT|CASCADE)/i )
        {
            $table_name = $1;
            $self->{struct}->{drop_behavior} = uc $2;
        }
    }
    else
    {
        return $self->do_err("Incomplete DROP statement!");

    }
    return undef unless $self->TABLE_NAME($table_name);
    $table_name = $self->replace_quoted_ids($table_name);
    $self->{tmp}->{is_table_name} = { $table_name => 1 };
    $self->{struct}->{table_names} = [$table_name];
    return 1;
}

####################################################
# DELETE FROM <table_name> WHERE <search_condition>
####################################################
sub DELETE
{
    my ( $self, $str ) = @_;
    $self->{struct}->{command} = 'DELETE';
    $str =~ s/^DELETE\s+FROM\s+/DELETE /i;    # Make FROM optional
    my ( $table_name, $where_clause ) = $str =~ /^DELETE (\S+)(.*)$/i;
    return $self->do_err('Incomplete DELETE statement!') if !$table_name;
    return undef unless $self->TABLE_NAME($table_name);
    $self->{tmp}->{is_table_name}  = { $table_name => 1 };
    $self->{struct}->{table_names} = [$table_name];
    $self->{struct}->{column_defs} = [
        {
            type  => 'column',
            value => '*'
        }
    ];
    $where_clause =~ s/^\s+//;
    $where_clause =~ s/\s+$//;

    if ($where_clause)
    {
        $where_clause =~ s/^WHERE\s*(.*)$/$1/i;
        return undef unless $self->SEARCH_CONDITION($where_clause);
    }
    return 1;
}

##############################################################
# SELECT
##############################################################
#    SELECT [<set_quantifier>] <select_list>
#           | <set_function_specification>
#      FROM <from_clause>
#    [WHERE <search_condition>]
# [ORDER BY <order_by_clause>]
#    [LIMIT <limit_clause>]
##############################################################

sub SELECT
{
    my ( $self, $str ) = @_;
    $self->{struct}->{command} = 'SELECT';
    my ( $from_clause, $where_clause, $order_clause, $groupby_clause, $limit_clause );
    $str =~ s/^SELECT (.+)$/$1/i;
    if ( $str =~ s/^(.+) LIMIT (.+)$/$1/i )    { $limit_clause   = $2; }
    if ( $str =~ s/^(.+) ORDER BY (.+)$/$1/i ) { $order_clause   = $2; }
    if ( $str =~ s/^(.+) GROUP BY (.+)$/$1/i ) { $groupby_clause = $2; }
    if ( $str =~ s/^(.+?) WHERE (.+)$/$1/i )   { $where_clause   = $2; }
    if ( $str =~ s/^(.+?) FROM (.+)$/$1/i )    { $from_clause    = $2; }

    #    else {
    #        return $self->do_err("Couldn't find FROM clause in SELECT!");
    #    }
    #    return undef unless $self->FROM_CLAUSE($from_clause);
    my $has_from_clause = $self->FROM_CLAUSE($from_clause) if ($from_clause);

    return undef unless ( $self->SELECT_CLAUSE($str) );

    if ($where_clause)
    {
        return undef unless ( $self->SEARCH_CONDITION($where_clause) );
    }
    if ($groupby_clause)
    {
        return undef unless ( $self->GROUPBY_LIST($groupby_clause) );
    }
    if ($order_clause)
    {
        return undef unless ( $self->SORT_SPEC_LIST($order_clause) );
    }
    if ($limit_clause)
    {
        return undef unless ( $self->LIMIT_CLAUSE($limit_clause) );
    }
    if (
        ( $self->{struct}->{join}->{clause} and $self->{struct}->{join}->{clause} eq 'ON' )
        or ( $self->{struct}->{multiple_tables}
            and !( scalar keys %{ $self->{struct}->{join} } ) )
      )
    {
        return undef unless ( $self->IMPLICIT_JOIN() );
    }

    if (   $self->{struct}->{set_quantifier}
        && ( 'DISTINCT' eq $self->{struct}->{set_quantifier} )
        && $self->{struct}->{has_set_functions}
        && !defined( _ARRAY( $self->{struct}->{group_by} ) ) )
    {
        delete $self->{struct}->{set_quantifier};
        carp "Specifying DISTINCT when using aggregate functions isn't reasonable - ignored."
          if ( $self->{PrintError} );
    }

    return 1;
}

sub GROUPBY_LIST
{
    my ( $self, $gclause ) = @_;
    return 1 unless ($gclause);
    my $cols = $self->ROW_VALUE_LIST($gclause);
    return undef if ( $self->{struct}->{errstr} );
    @{ $self->{struct}->{group_by} } = map { $_->{fullorg} } @{$cols};
    return 1;
}

sub IMPLICIT_JOIN
{
    my $self = $_[0];
    delete $self->{struct}->{multiple_tables};
    if (  !$self->{struct}->{join}->{clause}
        or $self->{struct}->{join}->{clause} ne 'ON' )
    {
        $self->{struct}->{join}->{type}   = 'INNER';
        $self->{struct}->{join}->{clause} = 'IMPLICIT';
    }
    if ( defined $self->{struct}->{keycols} )
    {
        my @keys;
        my @keys2 = @keys = @{ $self->{struct}->{keycols} };
        $self->{struct}->{join}->{table_order} = $self->order_joins( \@keys2 );
        @{ $self->{struct}->{join}->{keycols} } = @keys;
        delete $self->{struct}->{keycols};
    }
    else
    {
        return $self->do_err("No equijoin condition in WHERE or ON clause");
    }
    return 1;
}

sub EXPLICIT_JOIN
{
    my ( $self, $remainder ) = @_;
    return undef unless ($remainder);
    my ( $tableA, $tableB, $keycols, $jtype, $natural );
    if ( $remainder =~ m/^(.+?) (NATURAL|INNER|LEFT|RIGHT|FULL|CROSS|UNION|JOIN)(.+)$/is )
    {
        $tableA    = $1;
        $remainder = $2 . $3;
    }
    else
    {
        ( $tableA, $remainder ) = $remainder =~ m/^(\S+) (.*)/i;
    }
    if ( $remainder =~ m/^NATURAL (.+)/ )
    {
        $self->{struct}->{join}->{clause} = 'NATURAL';
        $natural++;
        $remainder = $1;
    }
    if ( $remainder =~ m/^(INNER|LEFT|RIGHT|FULL|CROSS|UNION) JOIN (.+)/i )
    {
        $jtype = $self->{struct}->{join}->{clause} = uc($1);
        $remainder = $2;
        $jtype = "$jtype OUTER" if $jtype !~ /INNER|UNION/i;
    }
    if ( $remainder =~ m/^(LEFT|RIGHT|FULL|CROSS) OUTER JOIN (.+)/i )
    {
        $jtype = $self->{struct}->{join}->{clause} = uc($1) . " OUTER";
        $remainder = $2;
    }
    if ( $remainder =~ m/^JOIN (.+)/i )
    {
        $jtype                            = 'INNER';
        $self->{struct}->{join}->{clause} = 'DEFAULT INNER';
        $remainder                        = $1;
    }
    if ( $self->{struct}->{join} )
    {
        if ( $remainder && $remainder =~ m/^(.+?) USING \(([^\)]+)\)(.*)/i )
        {
            $self->{struct}->{join}->{clause} = 'USING';
            $tableB = $1;
            my $keycolstr = $2;
            $remainder = $3;
            @$keycols = split( /,/, $keycolstr );
        }
        if ( $remainder && $remainder =~ m/^(.+?) ON (.+)/i )
        {
            $self->{struct}->{join}->{clause} = 'ON';
            $tableB = $1;
            my $keycolstr = $2;
            $remainder = $3;
            @$keycols = split(/ AND|OR /i, $keycolstr);

            return undef
              unless $self->TABLE_NAME_LIST( $tableA . ',' . $tableB );

            #              $self->{tmp}->{is_table_name}->{"$tableA"} = 1;
            #              $self->{tmp}->{is_table_name}->{"$tableB"} = 1;
            for my $keycol (@$keycols)
            {
                my %is_done;
		$keycol =~ s/\)|\(//g;
                my ( $arg1, $arg2 ) = split( m/ [>=<] /, $keycol );
                my ( $c1, $c2 ) = ( $arg1, $arg2 );
                $c1 =~ s/^.*\.([^\.]+)$/$1/;
                $c2 =~ s/^.*\.([^\.]+)$/$1/;
                if ( $c1 eq $c2 )
                {
                    return undef unless ( $arg1 = $self->ROW_VALUE($c1) );
                    if ( $arg1->{type} eq 'column' and !$is_done{$c1} )
                    {
                        push( @{ $self->{struct}->{keycols} }, $arg1->{value} );
                        $is_done{$c1} = 1;
                    }
                }
                else
                {
                    return undef unless ( $arg1 = $self->ROW_VALUE($arg1) );
                    return undef unless ( $arg2 = $self->ROW_VALUE($arg2) );
                    if (    $arg1->{type} eq 'column'
                        and $arg2->{type} eq 'column' )
                    {
                        push( @{ $self->{struct}->{keycols} }, $arg1->{value} );
                        push( @{ $self->{struct}->{keycols} }, $arg2->{value} );

                        # delete $self->{struct}->{where_clause};
                    }
                }
            }
        }
        elsif ( $remainder =~ /^(.+?)$/i )
        {
            $tableB    = $1;
            $remainder = $2;
        }
        $remainder =~ s/^\s+// if ($remainder);
    }

    if ($jtype)
    {
        $jtype = "NATURAL $jtype" if ($natural);
        if ( $natural and $keycols )
        {
            return $self->do_err(qq{Can't use NATURAL with a USING or ON clause!});
        }
        return undef unless ( $self->TABLE_NAME_LIST("$tableA,$tableB") );
        $self->{struct}->{join}->{type} = $jtype;
        $self->{struct}->{join}->{keycols} = $keycols if ($keycols);
        return 1;
    }
    return $self->do_err("Couldn't parse explicit JOIN!");
}

sub SELECT_CLAUSE
{
    my ( $self, $str ) = @_;
    return undef unless ($str);
    if ( $str =~ s/^(DISTINCT|ALL) (.+)$/$2/i )
    {
        $self->{struct}->{set_quantifier} = uc($1);
    }
    return undef unless ( $self->SELECT_LIST($str) );
}

sub FROM_CLAUSE
{
    my ( $self, $str ) = @_;
    return undef unless $str;
    if ( $str =~ m/ JOIN /i )
    {
        return undef unless $self->EXPLICIT_JOIN($str);
    }
    else
    {
        return undef unless $self->TABLE_NAME_LIST($str);
    }
}

sub INSERT
{
    my ( $self, $str ) = @_;
    my $col_str;
    $str =~ s/^INSERT\s+INTO\s+/INSERT /i;    # allow INTO to be optional
    my ( $table_name, $val_str ) = $str =~ m/^INSERT\s+(.+?)\s+VALUES\s+(\(.+\))$/i;
    if ( $table_name and $table_name =~ m/[()]/ )
    {
        ( $table_name, $col_str, $val_str ) = $str =~ m/^INSERT\s+(.+?)\s+\((.+?)\)\s+VALUES\s+(\(.+\))$/i;
    }
    return $self->do_err('No table name specified!') unless ($table_name);
    return $self->do_err('Missing values list!')     unless ( defined $val_str );
    return undef                                     unless ( $self->TABLE_NAME($table_name) );
    $self->{struct}->{command}     = 'INSERT';
    $self->{struct}->{table_names} = [$table_name];
    if ($col_str)
    {
        return undef unless ( $self->{struct}->{column_defs} = $self->ROW_VALUE_LIST($col_str) );
    }
    else
    {
        $self->{struct}->{column_defs} = [
            {
                type  => 'column',
                value => '*'
            }
        ];
    }
    $self->{struct}->{values} = [];
    for (my ($v,$line_str) = $val_str;
	 (($line_str,$v)=extract_bracketed($v,"('",'')) && defined $line_str;
	) {
      return undef unless ( $self->LITERAL_LIST(substr($line_str,1,-1)) );
      last unless $v =~ s/\A\s*,\s*//;
    }

    return 1;
}

###################################################################
# UPDATE ::=
#
# UPDATE <table> SET <set_clause_list> [ WHERE <search_condition>]
#
###################################################################
sub UPDATE
{
    my ( $self, $str ) = @_;
    $self->{struct}->{command} = 'UPDATE';
    my ( $table_name, $remainder ) = $str =~ m/^UPDATE (.+?) SET (.+)$/i;
    return $self->do_err('Incomplete UPDATE clause') unless ( $table_name && $remainder );
    return undef unless ( $self->TABLE_NAME($table_name) );
    $self->{tmp}->{is_table_name} = { $table_name => 1 };
    $self->{struct}->{table_names} = [$table_name];
    my ( $set_clause, $where_clause ) = $remainder =~ m/(.*?) WHERE (.*)$/i;
    $set_clause = $remainder if ( !$set_clause );
    return undef unless ( $self->SET_CLAUSE_LIST($set_clause) );

    if ($where_clause)
    {
        return undef unless ( $self->SEARCH_CONDITION($where_clause) );
    }

    my @vals                 = @{ $self->{struct}->{values}->[0] };
    my $num_val_placeholders = 0;
    for my $v (@vals)
    {
        ++$num_val_placeholders if ( $v->{type} eq 'placeholder' );
    }
    $self->{struct}->{num_val_placeholders} = $num_val_placeholders;

    return 1;
}

############
# FUNCTIONS
############
sub LOAD
{
    my ( $self, $str ) = @_;
    $self->{struct}->{command}    = 'LOAD';
    $self->{struct}->{no_execute} = 1;
    my ($package) = $str =~ /^LOAD\s+(.+)$/;
    $str = $package;
    $package =~ s/\?(\d+)\?/$self->{struct}->{literals}->[$1]/g;

    $self->_load_class($package);

    my %subs = eval '%' . $package . '::';

    for my $sub ( keys %subs )
    {
        next unless ( $sub =~ m/^SQL_FUNCTION_([A-Z_0-9]+)$/ );
        my $funcName = uc $1;
        my $subname  = $package . '::' . 'SQL_FUNCTION_' . $funcName;
        $self->{opts}->{function_names}->{$funcName} = $subname;
        delete $self->{opts}->{_udf_function_names};
    }
    1;
}

sub CREATE_RAM_TABLE
{
    my ( $self, $stmt ) = @_;
    $self->{struct}->{is_ram_table} = 1;
    $self->{struct}->{command}      = 'CREATE_RAM_TABLE';
    my ( $table_name, $table_element_def, %is_col_name );
    if ( $stmt =~ /^(\S+)\s+LIKE\s*(.+)$/si )
    {
        $table_name        = $1;
        $table_element_def = $2;
        if ( $table_element_def =~ /^(.*)\s+KEEP CONNECTION\s*$/i )
        {
            $table_element_def = $1;
            $self->{struct}->{ram_table_keep_connection} = 1;
        }
    }
    else
    {
        return $self->CREATE("CREATE TABLE $stmt");
    }
    return undef unless $self->TABLE_NAME($table_name);
    for my $col ( split ',', $table_element_def )
    {
        push( @{ $self->{struct}->{column_defs} }, $self->ROW_VALUE($col) );
    }
    $self->{struct}->{table_names} = [$table_name];
    return 1;
}

sub CREATE_FUNCTION
{
    my ( $self, $stmt ) = @_;
    $self->{struct}->{command}    = 'CREATE_FUNCTION';
    $self->{struct}->{no_execute} = 1;
    my ( $func, $subname );
    $stmt =~ s/\s*EXTERNAL//i;
    if ( $stmt =~ /^(\S+)\s+NAME\s+(.*)$/smi )
    {
        $func    = trim($1);
        $subname = trim($2);
    }
    $func    ||= $stmt;
    $subname ||= $func;
    if ( $func =~ /^\?QI(\d+)\?$/ )
    {
        $func = $self->{struct}->{quoted_ids}->[$1];
    }
    if ( $subname =~ /^\?QI(\d+)\?$/ )
    {
        $subname = $self->{struct}->{quoted_ids}->[$1];
    }
    $self->{opts}->{function_names}->{ uc $func } = $subname;
    delete $self->{opts}->{_udf_function_names};

    return 1;
}

sub CALL
{
    my ( $self, $stmt ) = @_;
    $stmt =~ s/^CALL\s+(.*)/$1/i;
    $self->{struct}->{command}   = 'CALL';
    $self->{struct}->{procedure} = $self->ROW_VALUE($stmt);
    return 1;
}

sub CREATE_TYPE
{
    my ( $self, $type ) = @_;
    $self->{struct}->{command}    = 'CREATE_TYPE';
    $self->{struct}->{no_execute} = 1;
    $self->feature( 'valid_data_types', uc $type, 1 );
}

sub DROP_TYPE
{
    my ( $self, $type ) = @_;
    $self->{struct}->{command}    = 'DROP_TYPE';
    $self->{struct}->{no_execute} = 1;
    $self->feature( 'valid_data_types', uc $type, 0 );
}

sub CREATE_KEYWORD
{
    my ( $self, $type ) = @_;
    $self->{struct}->{command}    = 'CREATE_KEYWORD';
    $self->{struct}->{no_execute} = 1;
    $self->feature( 'reserved_words', uc $type, 1 );
}

sub DROP_KEYWORD
{
    my ( $self, $type ) = @_;
    $self->{struct}->{command}    = 'DROP_KEYWORD';
    $self->{struct}->{no_execute} = 1;
    $self->feature( 'reserved_words', uc $type, 0 );
}

sub CREATE_OPERATOR
{
    my ( $self, $stmt ) = @_;
    $self->{struct}->{command}    = 'CREATE_OPERATOR';
    $self->{struct}->{no_execute} = 1;

    my ( $func, $subname );
    $stmt =~ s/\s*EXTERNAL//i;
    if ( $stmt =~ /^(\S+)\s+NAME\s+(.*)$/smi )
    {
        $func    = trim($1);
        $subname = trim($2);
    }
    $func    ||= $stmt;
    $subname ||= $func;
    if ( $func =~ /^\?QI(\d+)\?$/ )
    {
        $func = $self->{struct}->{quoted_ids}->[$1];
    }
    if ( $subname =~ /^\?QI(\d+)\?$/ )
    {
        $subname = $self->{struct}->{quoted_ids}->[$1];
    }
    $self->{opts}->{function_names}->{ uc $func } = $subname;
    delete $self->{opts}->{_udf_function_names};

    $self->feature( 'valid_comparison_operators', uc $func, 1 );
    return $self->create_op_regexen();
}

sub DROP_OPERATOR
{
    my ( $self, $type ) = @_;
    $self->{struct}->{command}    = 'DROP_OPERATOR';
    $self->{struct}->{no_execute} = 1;
    $self->feature( 'valid_comparison_operators', uc $type, 0 );
    return $self->create_op_regexen();
}

sub replace_quoted($)
{
    my ( $self, $str ) = @_;
    my @l = map { $self->replace_quoted_ids($_) } split( ',', $self->replace_quoted_commas($str) );
    return @l;
}

#########
# CREATE
#########
sub CREATE
{
    my ( $self, $stmt ) = @_;
    my $features = 'TYPE|KEYWORD|FUNCTION|OPERATOR|PREDICATE';
    if ( $stmt =~ m/^\s*CREATE\s+($features)\s+(.+)$/si )
    {
        my ( $sub, $arg ) = ( $1, $2 );
        $sub = 'CREATE_' . uc $sub;
        return $self->$sub($arg);
    }

    $stmt =~ s/^CREATE (LOCAL|GLOBAL) /CREATE /si;
    if ( $stmt =~ m/^\s*CREATE\s+(?:TEMP|TEMPORARY)\s+TABLE\s+(.+)$/si )
    {
        $stmt = "CREATE TABLE $1";
        $self->{struct}->{is_ram_table} = 1;
    }
    $self->{struct}->{command} = 'CREATE';
    my ( $table_name, $table_element_def, %is_col_name );

    if ( $stmt =~ m/^(.*) ON COMMIT (DELETE|PRESERVE) ROWS\s*$/si )
    {
        $stmt = $1;
        $self->{struct}->{commit_behaviour} = $2;

        #        return $self->do_err(
        #           "Can't specify commit behaviour for permanent tables."
        #        )
        #           if !defined $self->{struct}->{table_type}
        #              or $self->{struct}->{table_type} !~ /TEMPORARY/;
    }
    if ( $stmt =~ m/^CREATE TABLE (\S+) \((.*)\)$/si )
    {
        $table_name        = $1;
        $table_element_def = $2;
    }
    elsif ( $stmt =~ m/^CREATE TABLE (\S+) AS (.*)$/si )
    {
        $table_name = $1;
        my $subquery = $2;
        return undef unless $self->TABLE_NAME($table_name);
        $self->{struct}->{table_names} = [$table_name];

        # undo subquery replaces
        $subquery =~ s/\?(\d+)\?/'$self->{struct}{literals}[$1]'/g;
        $subquery =~ s/\?QI(\d+)\?/"$self->{struct}->{quoted_ids}->[$1]"/g;
        $subquery =~ s/\?COMMA\?/,/gs;
        $self->{struct}->{subquery} = $subquery;
        if ( -1 != index( $subquery, '?' ) )
        {
            ++$self->{struct}->{num_placeholders};
        }
        return 1;
    }
    else
    {
        return $self->do_err("Can't find column definitions!");
    }
    return undef unless ( $self->TABLE_NAME($table_name) );
    $table_element_def =~ s/\s+\(/(/g;
    my $primary_defined;
    while (
        $table_element_def =~ s/( # start of grouping 1
			   \( # match a bracket; vi compatible bracket -> \)(
                        [^)]+ # everything up to but not including the comma, no nesting of brackets is required
                            ) # end of grouping 1
                            , # the comma to be removed to allow splitting on commas
                            ( # start of grouping 2; vi compatible bracket -> \(
                        .*?\) # everything up to and including the end bracket
                            )/$1?COMMA?$2/sgx
      )
    {
    }

    for my $col ( split( ',', $table_element_def ) )
    {
        if (
            $col =~ m/^\s*(?:CONSTRAINT\s+(\S+)\s*)? # optional name of foreign key
                               FOREIGN\s+KEY\s*\(\s* # start of list of; vi compatibile bracket -> (
                                       (\s*[^)]+\s*) # field names in this table
                                            \s*\)\s* # end of field names in this table
                                          REFERENCES # key word
                                         \s*(\S+)\s* # table name being referenced in foreign key
                                               \(\s* # start of list of; vi compatible bracket -> (
                                       (\s*[^)]+\s*) # field names in foreign table
                                            \s*\)\s* # end of field names in foreign table
                    $/x
          )
        {
            my ( $name, $local_cols, $referenced_table, $referenced_cols ) = ( $1, $2, $3, $4 );
            my @local_cols = $self->replace_quoted($local_cols);
            $referenced_table = $self->replace_quoted_ids($referenced_table);
            my @referenced_cols = $self->replace_quoted($referenced_cols);

            if ( defined $name )
            {
                $name = $self->replace_quoted_ids($name);
            }
            else
            {
                $name = $self->replace_quoted_ids($table_name);
                my ($quote_char) = '';
                if ( $name =~ s/(\W)$// )
                {
                    $quote_char = ($1);
                }
                foreach my $local_col (@local_cols)
                {
                    my $col_name = $local_col;
                    $col_name =~ s/^\W//;
                    $col_name =~ s/\W$//;
                    $name .= '_' . $col_name;
                }
                $name .= '_fkey' . $quote_char;
            }

            $self->{struct}->{table_defs}->{$name}->{type}             = 'FOREIGN';
            $self->{struct}->{table_defs}->{$name}->{local_cols}       = \@local_cols;
            $self->{struct}->{table_defs}->{$name}->{referenced_table} = $referenced_table;
            $self->{struct}->{table_defs}->{$name}->{referenced_cols}  = \@referenced_cols;
            next;
        }
        elsif (
            $col =~ m/^\s*(?:CONSTRAINT\s+(\S+)\s*)? # optional name of foreign key
                               PRIMARY\s+KEY\s*\(\s* # start of list of; vi compatibile bracket -> (
                                       (\s*[^)]+\s*) # field names in this table
                                            \s*\)\s* # end of field names in this table
                        $/x
          )
        {
            my ( $name, $local_cols ) = ( $1, $2 );
            my @local_cols = $self->replace_quoted($local_cols);
            if ( defined $name )
            {
                $name = $self->replace_quoted_ids($name);
            }
            else
            {
                $name = $table_name;
                if ( $name =~ s/(\W)$// )
                {
                    $name .= '_pkey' . $1;
                }
                else
                {
                    $name .= '_pkey';
                }
            }
            $self->{struct}->{table_defs}->{$name}->{type}       = 'PRIMARY';
            $self->{struct}->{table_defs}->{$name}->{local_cols} = \@local_cols;
            next;
        }

        # it seems, perl 5.6 isn't greedy enough .. let's help a bit
        my ($data_types_regex) = join( '|', sort { length($b) <=> length($a) } keys %{ $self->{opts}->{valid_data_types} } );
        $data_types_regex =~ s/ /\\ /g;    # backslash spaces to allow the /x modifier below
        my ( $name, $type, $suffix ) = (
            $col =~ m/\s*(\S+)\s+                         # capture the column name
                        ((?:$data_types_regex|\S+)        # check for all allowed data types OR anything that looks like a bad data type to give a good error
                        (?:\s*\(\d+(?:\?COMMA\?\d+)?\))?) # allow the data type to have a precision specifier such as NUMERIC(4,6) on it
                        \s*(\W.*|$)                       # capture the suffix of the column definition, e.g. constraints
                     /ix
        );
        return $self->do_err("Column definition is missing a data type!") unless ($type);
        return undef unless ( $self->IDENTIFIER($name) );

        $name = $self->replace_quoted_ids($name);

        my @possible_constraints = ('PRIMARY KEY', 'NOT NULL', 'UNIQUE');

        for my $constraint (@possible_constraints)
        {
            my $count = $suffix =~ s/$constraint//gi;
            next if $count == 0;

            return $self->do_err(qq~Duplicate column constraint: '$constraint'!~)
                if $count > 1;

            return $self->do_err(qq{Can't have two PRIMARY KEYs in a table!})
                if $constraint eq 'PRIMARY KEY' and $primary_defined++;

            push @{ $self->{struct}->{table_defs}->{columns}->{$name}->{constraints} }, $constraint;
        }

        $suffix =~ s/^\s+//;
        $suffix =~ s/\s+$//;

        return $self->do_err("Unknown column constraint: '$suffix'!") unless ($suffix eq '');

        $type = uc $type;
        my $length;
        if ( $type =~ m/(.+)\((.+)\)/ )
        {
            $type   = $1;
            $length = $2;
        }
        if ( !$self->{opts}->{valid_data_types}->{$type} )
        {
            return $self->do_err("'$type' is not a recognized data type!");
        }
        $self->{struct}->{table_defs}->{columns}->{$name}->{data_type}   = $type;
        $self->{struct}->{table_defs}->{columns}->{$name}->{data_length} = $length;
        push(
            @{ $self->{struct}->{column_defs} },
            {
                type    => 'column',
                value   => $name,
                fullorg => $name,
            }
        );

        my $tmpname = $name;
        $tmpname = lc $tmpname unless ( $tmpname =~ m/^(?:\p{Word}+\.)?"/ );
        return $self->do_err("Duplicate column names!") if $is_col_name{$tmpname}++;

    }
    $self->{struct}->{table_names} = [$table_name];
    return 1;
}

###############
# SQL SUBRULES
###############

sub SET_CLAUSE_LIST
{
    my ( $self, $set_string ) = @_;
    my @sets = split( /,/, $set_string );
    my ( @cols, @vals );
    for my $set (@sets)
    {
        my ( $col, $val ) = split( m/ = /, $set );
        return $self->do_err('Incomplete SET clause!') unless ( defined($col) && defined($val) );
        push( @cols, $col );
        push( @vals, $val );
    }
    return undef
      unless ( $self->{struct}->{column_defs} = $self->ROW_VALUE_LIST( join ',', @cols ) );
    return undef unless ( $self->LITERAL_LIST( join ',', @vals ) );
    return 1;
}

sub SET_QUANTIFIER
{
    my ( $self, $str ) = @_;
    if ( $str =~ /^(DISTINCT|ALL)\s+(.*)$/si )
    {
        $self->{struct}->{set_quantifier} = uc $1;
        $str = $2;
    }
    return $str;
}

#
#	DAA v1.11
#	modify to transform || strings into
#	CONCAT(<expr>); note that we
#	only xform the topmost expressions;
#	if a concat is contained within a subfunction,
#	it should get handled by ROW_VALUE()
#
sub transform_concat
{
    my ( $obj, $colstr ) = @_;

    pos($colstr) = 0;
    my $parens  = 0;
    my $spos    = 0;
    my @concats = ();
    my $alias   = ( $colstr =~ s/^(.+)(\s+AS\s+\S+)$/$1/ ) ? $2 : '';

    while ( $colstr =~ /\G.*?([\(\)\|])/gcs )
    {
        if ( $1 eq '(' )
        {
            $parens++;
        }
        elsif ( $1 eq ')' )
        {
            $parens--;
        }
        elsif (( !$parens )
            && ( substr( $colstr, $-[1] + 1, 1 ) eq '|' ) )
        {

            #
            # its a concat outside of parens, push prior string on stack
            #
            push @concats, substr( $colstr, $spos, $-[1] - $spos );
            $spos = $+[1] + 1;
            pos($colstr) = $spos;
        }
    }

    #
    #	no concats, return original
    #
    return $colstr unless scalar @concats;

    #
    #	don't forget the last one!
    #
    push @concats, substr( $colstr, $spos );
    return 'CONCAT(' . join( ', ', @concats ) . ")$alias";
}

#
#	DAA v1.10
#	improved column list extraction
#	original doesn't seem to handle
#	commas within function argument lists
#
#	DAA v1.11
#	modify to transform || strings into
#	CONCAT(<expr-list>)
#
sub extract_column_list
{
    my ( $self, $colstr ) = @_;

    my @collist = ();
    pos($colstr) = 0;
    my $parens = 0;
    my $spos   = 0;
    while ( $colstr =~ m/\G.*?([\(\),])/gcs )
    {
        if ( $1 eq '(' )
        {
            $parens++;
        }
        elsif ( $1 eq ')' )
        {
            $parens--;
        }
        elsif ( !$parens )
        {    # its a comma outside of parens
            push( @collist, substr( $colstr, $spos, $-[1] - $spos ) );
            $collist[-1] =~ s/^\s+//;
            $collist[-1] =~ s/\s+$//;
            return $self->do_err('Bad column list!') if ( $collist[-1] eq '' );
            $spos = $+[1];
        }
    }
    return $self->do_err('Unbalanced parentheses!') if ($parens);

    # don't forget the last one!
    push( @collist, substr( $colstr, $spos ) );
    $collist[-1] =~ s/^\s+//;
    $collist[-1] =~ s/\s+$//;
    return $self->do_err('Bad column list!') if ( $collist[-1] eq '' );

    # scan for and convert string concats to CONCAT()
    foreach ( 0 .. $#collist )
    {
        $collist[$_] = $self->transform_concat( $collist[$_] ) if ( $collist[$_] =~ m/\|\|/ );
    }

    return @collist;
}

sub SELECT_LIST
{
    my ( $self, $col_str ) = @_;
    if ( $col_str =~ m/^\s*\*\s*$/ )
    {
        $self->{struct}->{column_defs} = [
            {
                type  => 'column',
                value => '*'
            }
        ];
        $self->{struct}->{column_aliases} = {};

        return 1;
    }
    my @col_list = $self->extract_column_list($col_str);
    return undef unless ( scalar(@col_list) );

    my ( @newcols, %aliases );
    for my $col (@col_list)
    {
        # DAA:
        # need better alias test here, since AS is a common
        # keyword that might be used in a function
        my ( $fld, $alias ) =
            ( $col =~ m/^(.+?)\s+(?:AS\s+)?([A-Z]\p{Word}*|\?QI\d+\?)$/i )
          ? ( $1, $2 )
          : ( $col, undef );
        $col = $fld;
        if ( $col =~ m/^(\S+)\.\*$/ )
        {
            my $table = $1;
            if ( defined($alias) )
            {
                return $self->do_err("'$table.*' cannot be aliased");
            }
            $table = $self->{tmp}->{is_table_alias}->{$table}
              if ( $self->{tmp}->{is_table_alias}->{$table} );
            $table = $self->{tmp}->{is_table_alias}->{"\L$table"}
              if ( $self->{tmp}->{is_table_alias}->{"\L$table"} );
            return undef unless ( $self->TABLE_NAME($table) );
            $table = $self->replace_quoted_ids($table);
            push(
                @newcols,
                {
                    type  => 'column',
                    value => "$table.*",
                }
            );
        }
        else
        {
            my $newcol;
            $newcol = $self->SET_FUNCTION_SPEC($col);
            return if ( $self->{struct}->{errstr} );
            $newcol ||= $self->ROW_VALUE($col);
            return if ( $self->{struct}->{errstr} );
            return $self->do_err("Invalid SELECT entry '$col'")
              unless ( defined( _HASH($newcol) ) );

            # FIXME this might be better done later and only if not 2 functions with the same name are selected
            if ( !defined($alias)
                && ( ( 'function' eq $newcol->{type} ) || ( 'setfunc' eq $newcol->{type} ) ) )
            {
                $alias = $newcol->{name};
            }

            if ( defined($alias) )
            {
                $alias                                              = $self->replace_quoted_ids($alias);
                $newcol->{alias}                                    = $alias;
                $aliases{ $newcol->{fullorg} }                      = $alias;
                $self->{struct}->{ORG_NAME}->{ $newcol->{fullorg} } = $alias;
                $self->{struct}->{ALIASES}->{$alias}                = $newcol->{fullorg};
            }
            push( @newcols, $newcol );
        }
    }
    $self->{struct}->{column_aliases} = \%aliases;
    $self->{struct}->{column_defs}    = \@newcols;
    return 1;
}

sub SET_FUNCTION_SPEC
{
    my ( $self, $col_str ) = @_;

    if ( $col_str =~ m/^(COUNT|AVG|SUM|MAX|MIN) \((.*)\)\s*$/i )
    {
        my $set_function_name    = uc $1;
        my $set_function_arg_str = $2;
        my $distinct             = 'ALL';
        if ( $set_function_arg_str =~ s/(DISTINCT|ALL) (.+)$/$2/i )
        {
            $distinct = uc $1;
        }
        my $count_star = ( $set_function_name eq 'COUNT' ) && ( $set_function_arg_str eq '*' );

        my $set_function_arg;
        if ($count_star)
        {
            return $self->do_err("Keyword DISTINCT is not allowed for COUNT(*)")
              if ( 'DISTINCT' eq $distinct );
            $set_function_arg = {
                type  => 'column',
                value => '*'
            };
        }
        else
        {
            $set_function_arg = $self->ROW_VALUE($set_function_arg_str);
            return if ( $self->{struct}->{errstr} );
            return unless ( defined( _HASH($set_function_arg) ) );
        }

        $self->{struct}->{has_set_functions} = 1;

        my $value = {
            name     => $set_function_name,
            arg      => $set_function_arg,
            argstr   => lc($set_function_arg_str),
            distinct => $distinct,
            type     => 'setfunc',
            fullorg  => $col_str,
        };
        return $value;
    }
    else
    {
        return undef;
    }
}

sub LIMIT_CLAUSE
{
    my ( $self, $limit_clause ) = @_;

    #    $limit_clause = trim($limit_clause);
    $limit_clause =~ s/^\s+//;
    $limit_clause =~ s/\s+$//;

    return 1 if !$limit_clause;
    my $offset;
    my $limit;
    my $junk;
($offset, $limit, $junk ) = split /,|OFFSET/i, $limit_clause;
    if ($limit_clause =~ m/(\d+)\s+OFFSET\s+(\d+)/) {
	$limit = $1;
	$offset = $2;
    } else {
	( $offset, $limit, $junk ) = split /,/i, $limit_clause;
    }
    return $self->do_err('Bad limit clause!:'.$limit_clause)
      if ( defined $limit and $limit =~ /[^\d]/ )
      or ( defined $offset and $offset =~ /[^\d]/ )
      or defined $junk;
    if ( defined $offset and !defined $limit )
    {
        $limit = $offset;
        undef $offset;
    }
    $self->{struct}->{limit_clause} = {
        limit  => $limit,
        offset => $offset,
    };
    return 1;
}

sub SORT_SPEC_LIST
{
    my ( $self, $order_clause ) = @_;
    return 1 if !$order_clause;
    my @ocols;
    my @order_columns = split ',', $order_clause;
    for my $col (@order_columns)
    {
        my $newcol;
        my $newarg;
        if ( $col =~ /\s*(\S+)\s+(ASC|DESC)/si )
        {
            $newcol = $1;
            $newarg = uc $2;
        }
        elsif ( $col =~ /^\s*(\S+)\s*$/si )
        {
            $newcol = $1;
            $newarg = 'ASC';
        }
        else
        {
            return $self->do_err('Junk after column name in ORDER BY clause!');
        }
        $newcol = $self->COLUMN_NAME($newcol) or return;
        if ( $newcol =~ /^(.+)\..+$/s )
        {
            my $table = $1;
            $self->_verify_tablename( $table, "ORDER BY" );
        }
        push( @ocols, { $newcol => $newarg } );
    }
    $self->{struct}->{sort_spec_list} = \@ocols;
    return 1;
}

sub SEARCH_CONDITION
{
    my ( $self, $str ) = @_;
    $str =~ s/^\s*WHERE (.+)/$1/;
    $str =~ s/^\s+//;
    $str =~ s/\s+$//;
    return $self->do_err("Couldn't find WHERE clause!") unless $str;

    #
    #	DAA
    #	make these OO so subclasses can override them
    #
    $str = $self->repl_btwin($str);

    #
    #	DAA
    #	add another abstract method so subclasses
    #	can inject their own syntax transforms
    #
    $str = $self->transform_syntax($str);

    my $open_parens  = $str =~ tr/\(//;
    my $close_parens = $str =~ tr/\)//;
    if ( $open_parens != $close_parens )
    {
        return $self->do_err("Mismatched parentheses in WHERE clause!");
    }
    $str = nongroup_numeric( $self->nongroup_string($str) );
    my $pred =
        $open_parens
      ? $self->parens_search( $str, [] )
      : $self->non_parens_search( $str, [] );
    return $self->do_err("Couldn't find predicate!") unless $pred;
    $self->{struct}->{where_clause} = $pred;
    return 1;
}

############################################################
# UTILITY FUNCTIONS CALLED TO PARSE PARENS IN WHERE CLAUSE
############################################################

sub repl_btwin
{
    my ( $self, $str ) = @_;    # DAA make OO for subclassing
    my @lids;

    my $i = -1;
    while ( $str =~ m/\G.*(?:IN|BETWEEN)\s+\(/g )
    {
        my $start   = pos($str) - 1;
        my $lparens = 1;
        my $rparens = 0;
        while ( $str =~ m/\G.*?([\(\)])/gcs )
        {
            ++$lparens if ( '(' eq $1 );
            ++$rparens if ( ')' eq $1 );
            last       if ( $lparens == $rparens );
        }
        my $now = pos($str);
        ++$i;
        my $subst = "?LI$i?";
        my $term = substr( $str, $start, $now - $start, $subst );
        $term = substr( $term, 1, length($term) - 2 );
        push( @lids, $term );
        pos($str) = $start + length($subst);
    }

    $self->{struct}->{list_ids} = \@lids;
    return $str;
}

# groups clauses by nested parens
#
#	DAA
#	rewrite to correct paren scan
#	and optimize code, and remove
#	recursion
#
sub parens_search
{
    my ( $self, $str, $predicates ) = @_;
    my $index = scalar( @{$predicates} );

    # to handle WHERE (a=b) AND (c=d)
    # but needs escape space to not foul up AND/OR

    #	locate all open parens
    #	locate all close parens
    #	apply non_paren_search to contents of
    #	inner parens

    my $lparens = ( $str =~ tr/\(// );
    my $rparens = ( $str =~ tr/\)// );
    return $self->do_err( 'Unmatched ' . ( ( $lparens > $rparens ) ? 'left' : 'right' ) . " parentheses in '$str'!" )
      unless ( $lparens == $rparens );

    return $self->non_parens_search( $str, $predicates )
      unless $lparens;

    my @lparens = ();
    while ( $str =~ m/\G.*?([\(\)])/gcs )
    {
        push( @lparens, $-[1] ), next
          if ( $1 eq '(' );

        #
        #	got a close paren, so pop the position of matching
        #	left paren and extract the expression, removing the
        #	parens
        #
        my $pos     = pop @lparens;
        my $predlen = $+[1] - $pos;
        my $pred    = substr( $str, $pos + 1, $predlen - 2 );

        #
        #	note that this will pass thru any prior ^$index^ xlation,
        #	so we don't need to recurse to recover the predicate
        #
        substr( $str, $pos, $predlen ) = $pred, pos($str) = $pos + length($pred), next
          unless ( $pred =~ / (AND|OR) /i );

        #
        #	handle AND/OR
        #
        push( @$predicates, substr( $str, $pos + 1, $predlen - 2 ) );
        my $replacement = "^$#$predicates^";
        substr( $str, $pos, $predlen ) = $replacement;
        pos($str) = $pos + length($replacement);
    }

    return $self->non_parens_search( $str, $predicates );
}

# creates predicates from clauses that either have no parens
# or ANDs or have been previously grouped by parens and ANDs
#
#	DAA
#	rewrite to fix paren scanning
#
sub non_parens_search
{
    my ( $self, $str, $predicates ) = @_;
    my $neg  = 0;
    my $nots = {};

    $neg = 1, $nots = { pred => 1 }
      if ( $str =~ s/^NOT (\^.+)$/$1/i );

    my ( $pred1, $pred2, $op );
    my $and_preds = [];
    ( $str, $and_preds ) = group_ands($str);
    $str = $and_preds->[$1]
      if $str =~ /^\s*~(\d+)~\s*$/;

    return $self->non_parens_search( $$predicates[$1], $predicates )
      if ( $str =~ /^\s*\^(\d+)\^\s*$/ );

    if ( $str =~ /\G(.*?)\s+(AND|OR)\s+(.*)$/igcs )
    {
        ( $pred1, $op, $pred2 ) = ( $1, $2, $3 );

        if ( $pred1 =~ /^\s*\^(\d+)\^\s*$/ )
        {
            $pred1 = $self->non_parens_search( $$predicates[$1], $predicates );
        }
        else
        {
            $pred1 =~ s/\~(\d+)\~$/$and_preds->[$1]/g;
            $pred1 = $self->non_parens_search( $pred1, $predicates );
        }

        #
        #	handle pred2 as a full predicate
        #
        $pred2 =~ s/\~(\d+)\~$/$and_preds->[$1]/g;
        $pred2 = $self->non_parens_search( $pred2, $predicates );

        return {
            neg  => $neg,
            nots => $nots,
            arg1 => $pred1,
            op   => uc $op,
            arg2 => $pred2,
        };
    }

    #
    #	terminal predicate
    #	need to check for singleton functions here
    #
    my $xstr = $str;
    my ( $k, $v );
    if ( $str =~ /^\s*([A-Z]\p{Word}*)\s*\[/gcs )
    {

        #
        #	we've got a function, check if its a singleton
        #
        my $parens = 1;
        my $spos   = $-[1];
        my $epos   = 0;
        $epos = $-[1], $parens += ( $1 eq '[' ) ? 1 : -1 while ( ( $parens > 0 ) && ( $str =~ /\G.*?([\[\]])/gcs ) );
        $k = substr( $str, $spos, $epos - $spos + 1 );
        $k =~ s/\?(\d+)\?/$self->{struct}{literals}[$1]/g;

        #
        #	for now we assume our parens are balanced
        #	now look for a predicate operator and a right operand
        #
        $v = $1, $v =~ s/\?(\d+)\?/$self->{struct}{literals}[$1]/g
          if ( $str =~ /\G\s+\S+\s*(.+)\s*$/gcs );
    }
    else
    {
        $xstr =~ s/\?(\d+)\?/$self->{struct}{literals}[$1]/g;
        ( $k, $v ) = $xstr =~ /^(\S+?)\s+\S+\s*(.+)\s*$/;
    }
    push @{ $self->{struct}{where_cols}{$k} }, $v
      if defined $k;
    return $self->PREDICATE($str);
}

# groups AND clauses that aren't already grouped by parens
#
sub group_ands
{
    my $str = shift;
    my $and_preds = shift || [];
    return ( $str, $and_preds )
      unless $str =~ / AND / and $str =~ / OR /;

    return $str, $and_preds
      unless ( $str =~ /^(.*?) AND (.*)$/i );

    my ( $front, $back ) = ( $1, $2 );
    my $index = scalar @$and_preds;
    $front = $1
      if ( $front =~ /^.* OR (.*)$/i );

    $back = $1
      if ( $back =~ /^(.*?) (OR|AND) .*$/i );

    my $newpred = "$front AND $back";
    push @$and_preds, $newpred;
    $str =~ s/\Q$newpred/~$index~/i;
    return group_ands( $str, $and_preds );
}

# replaces string function parens with square brackets
# e.g TRIM (foo) -> TRIM[foo]
#
#	DAA update to support UDFs
#	and remove recursion
#
sub nongroup_string
{
    my ( $self, $str ) = @_;

    #
    #	add in any user defined functions
    #
    my $f = join( '|', FUNCTION_NAMES, $self->_udf_function_names );

    #
    #	we need a scan here to permit arbitrarily nested paren
    #	arguments to functions
    #
    my $parens = 0;
    my $pos;
    my @lparens = ();
    while ( $str =~ /\G.*?((\b($f)\s*\()|[\(\)])/igcs )
    {
        if ( $1 eq ')' )
        {
            #
            #	close paren, see if any pending function open
            #	paren matches it
            #
            --$parens;
            $pos = $+[0], substr( $str, $+[0] - 1, 1 ) = ']', pos($str) = $pos, pop(@lparens)
              if ( @lparens && ( $lparens[-1] == $parens ) );
        }
        elsif ( $1 eq '(' )
        {

            #
            #	just an open paren, count it and go on
            #
            ++$parens;
        }
        else
        {

            #
            #	new function definition, capture its open paren
            #	also uppercase the function name
            #
            $pos = $+[0];
            substr( $str, $-[3], length($3) ) = uc $3;
            substr( $str, $+[0] - 1, 1 ) = '[';
            pos($str) = $pos;
            push @lparens, $parens;
            ++$parens;
        }
    }

    #	return $self->do_err('Unmatched ' .
    #		(($parens > 0) ? 'left' : 'right') . ' parentheses!')
    #		if $parens;
    #
    #	DAA
    #	remove scoped recursion
    #
    #	return ( $str =~ /($f)\s*\(/i ) ?
    #		nongroup_string($str) : $str;
    return $str;
}

# replaces math parens with square brackets
# e.g (4-(6+7)*9) -> MATH[4-MATH[6+7]*9]
#
sub nongroup_numeric
{
    my $str = $_[0];
    my $has_op;

    #
    #	DAA
    #	optimize regex
    #
    if ( $str =~ m/\(([\p{Word} \*\/\+\-\[\]\?]+)\)/ )
    {
        my $match = $1;
        if ( $match !~ m/(LIKE |IS|BETWEEN|IN)/i )
        {
            my $re = quotemeta($match);
            $str =~ s/\($re\)/MATH\[$match\]/;
        }
        else
        {
            $has_op++;
        }
    }

    #
    #	DAA
    #	remove scoped recursion
    #
    return ( !$has_op and $str =~ /\(([\p{Word} \*\/\+\-\[\]\?]+)\)/ )
      ? nongroup_numeric($str)
      : $str;
}
############################################################

#########################################################
# LITERAL_LIST ::= <literal> [,<literal>]
#########################################################
sub LITERAL_LIST
{
    my ( $self, $str ) = @_;
    my @tokens = split /,/, $str;
    my @values;
    for my $tok (@tokens)
    {
        my $val = $self->ROW_VALUE($tok);
        return $self->do_err(qq('$tok' is not a valid value or is not quoted!))
          unless $val;
        push @values, $val;
    }
    push( @{ $self->{struct}->{values} }, \@values );
    return 1;
}

#############################################################################
# LITERAL ::= <quoted_string> | <question mark> | <number> | NULL/TRUE/FALSE
#############################################################################
sub LITERAL
{
    my ( $self, $str ) = @_;

    #
    #	DAA
    #	strip parens (if any)
    #
    $str = $1 while ( $str =~ m/^\s*\(\s*(.+)\s*\)\s*$/ );

    return 'null'    if $str =~ m/^NULL$/i;              # NULL
    return 'boolean' if $str =~ m/^(?:TRUE|FALSE)$/i;    # TRUE/FALSE

    #    return 'empty_string' if $str =~ /^~E~$/i;    # NULL
    if ( $str eq '?' )
    {
        $self->{struct}->{num_placeholders}++;
        return 'placeholder';
    }

    #    return 'placeholder' if $str eq '?';   # placeholder question mark
    return 'string' if $str =~ m/^'.*'$/s;               # quoted string
         # return 'number' if $str =~ m/^[+-]?(?=\d|\.\d)\d*(\.\d*)?([Ee]([+-]?\d+))?$/; # number
    return 'number' if ( looks_like_number($str) );    # number

    return undef;
}
###################################################################
# PREDICATE
###################################################################
sub PREDICATE
{
    my ( $self, $str ) = @_;

    my ( $arg1, $op, $arg2, $opexp );

    $opexp = $self->{opts}{valid_comparison_NOT_ops_regex}, ( $arg1, $op, $arg2 ) = $str =~ /$opexp/i
      if $self->{opts}{valid_comparison_NOT_ops_regex};

    $opexp = $self->{opts}{valid_comparison_twochar_ops_regex}, ( $arg1, $op, $arg2 ) = $str =~ /$opexp/i
      if ( !defined($op)
        && $self->{opts}{valid_comparison_twochar_ops_regex} );

    $opexp = $self->{opts}{valid_comparison_ops_regex}, ( $arg1, $op, $arg2 ) = $str =~ /$opexp/i
      if ( !defined($op) && $self->{opts}{valid_comparison_ops_regex} );

    #
    ### USER-DEFINED PREDICATE
    #
    unless ( defined $arg1 && defined $op && defined $arg2 )
    {
        $arg1 = $str;
        $op   = 'USER_DEFINED';
        $arg2 = '';
    }

    $op = uc $op;

    #	my $uname = $self->is_func($arg1);
    #        if (!$uname) {
    #           $arg1 =~ s/^(\S+).*$/$1/;
    #	    return $self->do_err("Bad predicate: '$arg1'!");
    #        }

    my $negated = 0;    # boolean value showing if predicate is negated
    my %not;            # hash showing elements modified by NOT
                        #
                        # e.g. "NOT bar = foo"        -> %not = (arg1=>1)
                        #      "bar NOT LIKE foo"     -> %not = (op=>1)
                        #      "NOT bar NOT LIKE foo" -> %not = (arg1=>1,op=>1);
                        #      "NOT bar IS NOT NULL"  -> %not = (arg1=>1,op=>1);
                        #      "bar = foo"            -> %not = undef;
                        #
    $not{arg1}++
      if ( $arg1 =~ s/^NOT (.+)$/$1/i );

    $not{op}++
      if ( $op =~ s/^(.+) NOT$/$1/i
        || $op =~ s/^NOT (.+)$/$1/i );

    $negated = 1 if %not and scalar keys %not == 1;

    return undef unless $arg1 = $self->ROW_VALUE($arg1);

    if ( $op ne 'USER_DEFINED' )
    {    # USER-PREDICATE;
        return undef unless $arg2 = $self->ROW_VALUE($arg2);
    }
    else
    {

        #        $arg2 = $self->ROW_VALUE($arg2);
    }

    if (    defined( _HASH($arg1) )
        and defined( _HASH($arg2) )
        and ( ( $arg1->{type} || '' ) eq 'column' )
        and ( ( $arg2->{type} || '' ) eq 'column' )
        and ( $op eq '=' ) )
    {
        push( @{ $self->{struct}->{keycols} }, $arg1->{value} );
        push( @{ $self->{struct}->{keycols} }, $arg2->{value} );
    }

    return {
        neg  => $negated,
        nots => \%not,
        arg1 => $arg1,
        op   => $op,
        arg2 => $arg2,
    };
}

sub _udf_function_names
{
    $_[0]->{opts}->{_udf_function_names}
      or return $_[0]->{opts}->{_udf_function_names} = join( "|", map { uc $_ } keys %{ $_[0]->{opts}->{function_names} } );
    $_[0]->{opts}->{_udf_function_names};
}

sub undo_string_funcs
{
    my ( $self, $str ) = @_;
    my $f = join( '|', FUNCTION_NAMES, $self->_udf_function_names );

    #	eliminate recursion:
    #	we have to scan for closing brackets, since we may
    #	have intervening MATH elements with brackets
    my ( $brackets, $pos, @lbrackets ) = (0);
    while ( $str =~ /\G.*?((\b($f)\s*\[)|[\[\]])/igcs )
    {
        if ( $1 eq ']' )
        {
            #	close paren, see if any pending function open
            #	paren matches it
            $brackets--;
            $pos = $+[0], substr( $str, $+[0] - 1, 1 ) = ')', pos($str) = $pos, pop @lbrackets
              if ( @lbrackets && ( $lbrackets[-1] == $brackets ) );
        }
        elsif ( $1 eq '[' )
        {
            #	just an open paren, count it and go on
            $brackets++;
        }
        else
        {
            #	new function definition, capture its open paren
            #	also uppercase the function name
            $pos = $+[0];
            substr( $str, $-[3], length($3) ) = uc $3;
            substr( $str, $+[0] - 1, 1 ) = '(';
            pos($str) = $pos;
            push @lbrackets, $brackets;
            $brackets++;
        }
    }

    return $str;
}

sub undo_math_funcs
{
    my $str = $_[0];

    #	eliminate recursion
    while ( $str =~ s/MATH\[([^\]\[]+?)\]/($1)/ )
    {
    }

    return $str;
}

#
#	DAA
#	need better nested function/parens handling
#
sub extract_func_args
{
    my ( $self, $value ) = @_;

    my @final_args = ();
    my ( $spos, $parens, $epos, $delim ) = ( 0, 0, 0, 0 );
    while ( $value =~ m/\G.*?([\(\),])/gcs )
    {
        $epos  = $+[0];
        $delim = $1;
        unless ( $parens or ( $delim ne ',' ) )
        {
            push( @final_args, $self->ROW_VALUE( substr( $value, $spos, $epos - $spos - 1 ) ) );
            $spos = $epos;
            next;
        }

        unless ( $delim eq ',' )
        {
            $parens += ( $delim eq '(' ) ? 1 : -1;
        }
    }

    #	don't forget the last argument
    if ( $spos != length($value) )
    {
        $epos = length($value);
        push( @final_args, $self->ROW_VALUE( substr( $value, $spos, $epos - $spos ) ) );    # XXX
    }

    return @final_args;
}

###################################################################
# ROW_VALUE ::= <literal> | <column_name>
###################################################################
sub ROW_VALUE
{
    my ( $self, $str ) = @_;

    $str =~ s/^\s+//;
    $str =~ s/\s+$//;
    $str = $self->undo_string_funcs($str);
    $str = undo_math_funcs($str);
    my ( $orgstr, $f, $bf ) = ( $str, FUNCTION_NAMES, BAREWORD_FUNCTIONS );

    # USER-DEFINED FUNCTION
    my ( $user_func_name, $user_func_args, $is_func );

    #	DAA
    #	need better paren check here
    if ( $str =~ m/^([^\s\(]+)\s*(.*)\s*$/ )
    {
        $user_func_name = $1;
        $user_func_args = $2;

        # convert operator-like function to parenthetical format
        if (   ( $is_func = $self->is_func($user_func_name) )
            && ( $user_func_args !~ m/^\(.*\)$/ )
            && ( $is_func =~ /^(?:$bf)$/i ) )
        {
            $orgstr = $str = "$user_func_name ($user_func_args)";
        }
    }
    else
    {
        $user_func_name = $str;
        $user_func_name =~ s/^(\S+).*$/$1/;
        $user_func_args = '';
        $is_func        = $self->is_func($user_func_name);
    }

    # BLKB
    # Limiting the parens convert shortcut, so that "SELECT LOG(1), PI" works as a
    # two functions, and "SELECT x FROM log" works as a table
    undef $is_func if ( $is_func && $is_func !~ /^(?:$bf)$/i && $str !~ m/^\S+\s*\(.*\)\s*$/ );

    if ( $is_func && ( uc($is_func) !~ m/^($f)$/ ) )
    {
        my ( $name, $value ) = ( $user_func_name, '' );
        if ( $str =~ m/^(\S+)\s*\((.*)\)\s*$/ )
        {
            $name    = $1;
            $value   = $2;
            $is_func = $self->is_func($name);
        }

        if ($is_func)
        {
            #
            #	DAA
            #	need a better argument extractor, since it can
            #	contain arbitrary (possibly parenthesized)
            #	expressions/functions
            #
            #           if ($value =~ /\(/ ) {
            #               $value = $self->ROW_VALUE($value);
            #           }
            #           my @args = split ',',$value;

            my @final_args = $self->extract_func_args($value);
            my $usr_sub    = $self->{opts}->{function_names}->{$is_func};
            $self->{struct}->{procedure} = {};
            if ($usr_sub)
            {
                $value = {
                    type    => 'function',
                    name    => lc $name,
                    subname => $usr_sub,
                    value   => \@final_args,
                    fullorg => $orgstr,
                };

                return $value;
            }
        }
    }

    my $type;
    # MATH
    #
    if ( $str =~ m/[\*\+\-\/\%]/ )
    {
        my @vals;
        my $i            = -1;
        my $open_parens  = $str =~ tr/\(//;
        my $close_parens = $str =~ tr/\)//;
        if ( $open_parens != $close_parens )
        {
            return $self->do_err("Mismatched parentheses in term '$str'!");
        }

        # $str =~ s/([^\s\*\+\-\/\%\)\(]+)/push @vals,$1;++$i;"?$i?"/ge;
        while ( $str =~ m/\G.*?([^\s\*\+\-\/\%\)\(]+)/g )
        {
            my $term  = $1;
            my $start = pos($str) - length($term);
            if ( $self->is_func($term) )
            {
                my $lparens = 0;
                my $rparens = 0;
                while ( $str =~ m/\G.*?([\(\)])/gcs )
                {
                    ++$lparens if ( '(' eq $1 );
                    ++$rparens if ( ')' eq $1 );
                    last       if ( $lparens == $rparens );
                }
                my $now = pos($str);
                ++$i;
                $term = substr( $str, $start, $now - $start, "?$i?" );
                push( @vals, $term );
                pos($str) = $start + length("?$i?");
            }
            else
            {
                push( @vals, $term );
                ++$i;
                substr( $str, $start, length($term), "?$i?" );
                pos($str) = $start + length("?$i?");
            }
        }

        my @newvalues;
        foreach my $val (@vals)
        {
            my $newval = $self->ROW_VALUE($val);
            if ( $newval && $newval->{type} !~ m/number|column|placeholder|function/ )
            {
                return $self->do_err(qq[String '$val' not allowed in Numeric expression!]);
            }
            push( @newvalues, $newval );
        }

        return {
            type    => 'function',
            name    => 'numeric_exp',
            str     => $str,
            value   => \@newvalues,
            fullorg => $orgstr,
        };
    }

    # SUBSTRING (value FROM start [FOR length])
    #
    if ( $str =~ m/^SUBSTRING \((.+?) FROM (.+)\)\s*$/i )
    {
        my $name  = 'SUBSTRING';
        my $start = $2;
        my $value = $self->ROW_VALUE($1);
        my $length;
        if ( $start =~ /^(.+?) FOR (.+)$/i )
        {
            $start  = $1;
            $length = $2;
            $length = $self->ROW_VALUE($length);
        }
        $start = $self->ROW_VALUE($start);
        $str =~ s/\?(\d+)\?/$self->{struct}->{literals}->[$1]/g;
        if (   ( $start->{type} eq 'string' )
            or ( $start->{length} && ( $start->{length}->{type} eq 'string' ) ) )
        {
            return $self->do_err("Can't use a string as a SUBSTRING position: '$str'!");
        }
        return undef unless ($value);
        return $self->do_err("Can't use a number in SUBSTRING: '$str'!")
          if $value->{type} eq 'number';
        return {
            type    => 'function',
            name    => $name,
            value   => [$value],
            start   => $start,
            length  => $length,
            fullorg => $orgstr,
        };
    }

    # TRIM ( [ [TRAILING|LEADING|BOTH] ['char'] FROM ] value )
    #
    if ( $str =~ m/^(TRIM) \((.+)\)\s*$/i )
    {
        my $name  = uc $1;
        my $value = $2;
        my ( $trim_spec, $trim_char );
        if ( $value =~ m/^(.+) FROM ([^\(\)]+)$/i )
        {
            my $front = $1;
            $value = $2;
            if ( $front =~ m/^\s*(TRAILING|LEADING|BOTH)(.*)$/i )
            {
                $trim_spec = uc $1;
                $trim_char = $2;
                $trim_char =~ s/^\s+//;
                $trim_char =~ s/\s+$//;
                undef $trim_char if ( length($trim_char) == 0 );
            }
            else
            {
                $trim_char = $front;
                $trim_char =~ s/^\s+//;
                $trim_char =~ s/\s+$//;
            }
        }

        $trim_char ||= '';
        $trim_char =~ s/\?(\d+)\?/$self->{struct}->{literals}->[$1]/g;
        $value = $self->ROW_VALUE($value);
        return undef unless ($value);
        $str =~ s/\?(\d+)\?/$self->{struct}->{literals}->[$1]/g;
        my $value_type = $value->{type} if ref $value eq 'HASH';
        $value_type = $value->[0] if ( defined( _ARRAY($value) ) );
        return $self->do_err("Can't use a number in TRIM: '$str'!")
          if ( $value_type and $value_type eq 'number' );

        return {
            type      => 'function',
            name      => $name,
            value     => [$value],
            trim_spec => $trim_spec,
            trim_char => $trim_char,
            fullorg   => $orgstr,
        };
    }

    # UNKNOWN FUNCTION
    if ( $str =~ m/^(\S+) \(/ )
    {
        return $self->do_err("Unknown function '$1'");
    }

    # STRING CONCATENATION
    #
    if ( $str =~ m/\|\|/ )
    {
        my @vals = split( m/ \|\| /, $str );
        my @newvals;
        for my $val (@vals)
        {
            my $newval = $self->ROW_VALUE($val);
            return undef unless ($newval);
            return $self->do_err("Can't use a number in string concatenation: '$str'!")
              if ( $newval->{type} eq 'number' );
            push @newvals, $newval;
        }
        return {
            type    => 'function',
            name    => 'str_concat',
            value   => \@newvals,
            fullorg => $orgstr,
        };
    }

    # NULL, BOOLEAN, PLACEHOLDER, NUMBER
    #
    if ( $type = $self->LITERAL($str) )
    {
        undef $str if ( $type eq 'null' );
        $str = 1 if ( $type eq 'boolean' and $str =~ /^TRUE$/i );
        $str = 0 if ( $type eq 'boolean' and $str =~ /^FALSE$/i );

        #        if ($type eq 'empty_string') {
        #           $str = '';
        #           $type = 'string';
        #	}
        $str = '' if ( $str and $str eq q('') );
        return {
            type    => $type,
            value   => $str,
            fullorg => $orgstr,
        };
    }

    # QUOTED STRING LITERAL
    #
    if ( $str =~ m/\?(\d+)\?/ )
    {
        return {
            type    => 'string',
            value   => $self->{struct}->{literals}->[$1],
            fullorg => $self->{struct}->{literals}->[$1],
        };
    }
    elsif ( $str =~ /^\?LI(\d+)\?$/ )
    {
        return $self->ROW_VALUE_LIST( $self->{struct}->{list_ids}->[$1] );
    }

    # COLUMN NAME
    #
    return undef unless ( $str = $self->COLUMN_NAME($str) );

    if ( $str =~ m/^(.*)\./ )
    {
        my $table_name = $1;
        $self->_verify_tablename( $table_name, "WHERE" );
    }

    #    push @{ $self->{struct}->{where_cols}},$str
    #       unless $self->{tmp}->{where_cols}->{"$str"};
    ++$self->{tmp}->{where_cols}->{$str};
    return {
        type    => 'column',
        value   => $str,
        fullorg => $orgstr,
    };
}

#########################################################
# ROW_VALUE_LIST ::= <row_value> [,<row_value>...]
#########################################################
sub ROW_VALUE_LIST
{
    my ( $self, $row_str ) = @_;
    my @row_list = split ',', $row_str;
    if ( !( scalar @row_list ) )
    {
        return $self->do_err('Missing row value list!');
    }
    my @newvals;
    my $newval;
    for my $row_val (@row_list)
    {
        $row_val =~ s/^\s+//;
        $row_val =~ s/\s+$//;

        return undef if !( $newval = $self->ROW_VALUE($row_val) );
        push @newvals, $newval;
    }
    return \@newvals;
}

###############################################
# COLUMN NAME ::= [<table_name>.] <identifier>
###############################################

sub COLUMN_NAME
{
    my ( $self, $str ) = @_;
    my ( $table_name, $col_name );
    if ( $str =~ m/^\s*(\S+)\.(\S+)$/s )
    {
        ( $table_name, $col_name ) = ( $1, $2 );
        if ( !$self->{opts}->{valid_options}->{SELECT_MULTIPLE_TABLES} )
        {
            return $self->do_err('Dialect does not support multiple tables!');
        }
        return undef unless ( $table_name = $self->TABLE_NAME($table_name) );
        $table_name = $self->replace_quoted_ids($table_name);
        $self->_verify_tablename($table_name);
    }
    else
    {
        $col_name = $str;
    }

    $col_name =~ s/^\s+//;
    $col_name =~ s/\s+$//;

    my $user_func = $col_name;
    $user_func =~ s/^(\S+).*$/$1/;
    if ( $col_name !~ m/^(TRIM|SUBSTRING)$/i )
    {
        undef $user_func unless ( $self->{opts}->{function_names}->{ uc $user_func } );
    }
    if ( !$user_func )
    {
        return undef unless ( ( $col_name eq '*' ) || $self->IDENTIFIER($col_name) );
    }

    #
    # MAKE COL NAMES ALL UPPER CASE UNLESS IS DELIMITED IDENTIFIER
    my $orgcol = $col_name;

    if ( $col_name =~ m/^\?QI(\d+)\?$/ )
    {
        $col_name = '"' . $self->{struct}->{quoted_ids}->[$1] . '"';
    }
    else
    {
        $col_name = lc $col_name
          unless (
            ( $self->{struct}->{command} eq 'CREATE' )
            ##############################################
            #
            # JZ addition to RR's alias patch
            #
            or ( $col_name =~ m/^(?:\p{Word}+\.)?"/ )
          );

    }

    #
    $col_name = $self->{struct}->{column_aliases}->{$col_name}
      if ( $self->{struct}->{column_aliases}->{$col_name} );

    #    $orgcol = $self->replace_quoted_ids($orgcol);
    ##############################################

    if ($table_name)
    {
        my $alias = $self->{tmp}->{is_table_alias}->{"\L$table_name"};
        $table_name = $alias if ( defined($alias) );
        $table_name = lc $table_name unless ( $table_name =~ m/^"/ );
        $col_name = "$table_name.$col_name" if ( -1 == index( $col_name, '.' ) );
    }
    return $col_name;
}

#########################################################
# COLUMN NAME_LIST ::= <column_name> [,<column_name>...]
#########################################################
sub COLUMN_NAME_LIST
{
    my ( $self, $col_str ) = @_;

    my @col_list = split( ',', $col_str );
    return $self->do_err('Missing column name list!') unless ( scalar(@col_list) );

    my @newcols;
    for my $col (@col_list)
    {
        $col =~ s/^\s+//;
        $col =~ s/\s+$//;

        my $newcol;
        return undef unless ( $newcol = $self->COLUMN_NAME($col) );
        push( @newcols, $newcol );
    }

    return \@newcols;
}

#####################################################
# TABLE_NAME_LIST := <table_name> [,<table_name>...]
#####################################################
sub TABLE_NAME_LIST
{
    my ( $self, $table_name_str ) = @_;
    my %aliases = ();
    my @tables;
    $table_name_str =~ s/(\?\d+\?),/$1:/g;    # fudge commas in functions
    my @table_names = split ',', $table_name_str;
    if ( scalar @table_names > 1
        and !$self->{opts}->{valid_options}->{SELECT_MULTIPLE_TABLES} )
    {
        return $self->do_err('Dialect does not support multiple tables!');
    }

    my $bf = BAREWORD_FUNCTIONS;
    my %is_table_alias;
    for my $table_str (@table_names)
    {
        $table_str =~ s/(\?\d+\?):/$1,/g;     # unfudge commas in functions
        $table_str =~ s/\s+\(/\(/g;           # fudge spaces in functions
        my ( $table, $alias );
        my (@tstr) = split( m/\s+/, $table_str );
        if ( @tstr == 1 )
        {
            $table = $tstr[0];
        }
        elsif ( @tstr == 2 )
        {
            $table = $tstr[0];
            $alias = $tstr[1];
        }
        elsif ( @tstr == 3 )
        {
            return $self->do_err("Can't find alias in FROM clause!")
              unless ( uc( $tstr[1] ) eq 'AS' );
            $table = $tstr[0];
            $alias = $tstr[2];
        }
        else
        {
            return $self->do_err("Can't find table names in FROM clause!");
        }

        $table =~ s/\(/ \(/g;    # unfudge spaces in functions
        my $u_name = $table;
        $u_name =~ s/^(\S+)\s*(.*$)/$1/;
        my $u_args = $2;

        if (   ( $u_name = $self->is_func($u_name) )
            && ( $u_name =~ /^(?:$bf)$/i || $table =~ /^$u_name\s*\(/i ) )
        {
            $u_args = " $u_args" if ($u_args);
            my $u_func = $self->ROW_VALUE( $u_name . $u_args );
            $self->{struct}->{table_func}->{$u_name} = $u_func;
            $self->{struct}->{temp_table}            = 1;
            $table                                   = $u_name;
        }
        else
        {
            return undef unless ( $self->TABLE_NAME($table) );
        }

        $table = $self->replace_quoted_ids($table);
        push( @tables, $table =~ m/^"/ ? $table : $table );

        if ($alias)
        {
            return unless ( $self->TABLE_NAME($alias) );
            $alias = $self->replace_quoted_ids($alias);
            if ( $alias =~ m/^"/ )
            {
                push( @{ $aliases{$table} }, $alias );
                $is_table_alias{$alias} = $table;
            }
            else
            {
                push( @{ $aliases{$table} }, "\L$alias" );
                $is_table_alias{"\L$alias"} = $table;
            }
        }
    }
    my %is_table_name = map { $_ => 1 } @tables;
    $self->{tmp}->{is_table_alias}     = \%is_table_alias;
    $self->{tmp}->{is_table_name}      = \%is_table_name;
    $self->{struct}->{table_names}     = \@tables;
    $self->{struct}->{table_alias}     = \%aliases;
    $self->{struct}->{multiple_tables} = 1 if ( @tables > 1 );
    return 1;
}

sub is_func($)
{
    my ( $self, $name ) = @_;
    $name =~ s/^(\S+).*$/$1/;
    return $name if ( $self->{opts}->{function_names}->{$name} );
    return uc $name if ( $self->{opts}->{function_names}->{ uc $name } );
    undef;
}

#############################
# TABLE_NAME := <identifier>
#############################
sub TABLE_NAME
{
    my ( $self, $table_name ) = @_;
    if ( $table_name =~ m/^(.+?)\.([^\.]+)$/ )
    {
        my $schema = $1;    # ignored
        $table_name = $2;
    }
    if ( $table_name =~ m/\s*(\S+)\s+\S+/s )
    {
        return $self->do_err("Junk after table name '$1'!");
    }
    $table_name =~ s/\s+//s;
    if ( !$table_name )
    {
        return $self->do_err('No table name specified!');
    }
    return $table_name if ( $self->IDENTIFIER($table_name) );

    #    return undef if !($self->IDENTIFIER($table_name));
    #    return 1;
}

sub _verify_tablename
{
    my ( $self, $table_name, $location ) = @_;
    if ( defined($location) )
    {
        $location = " in $location";
    }
    else
    {
        $location = "";
    }

    if ( $table_name =~ m/^"/ )
    {
        if (    !$self->{tmp}->{is_table_name}->{$table_name}
            and !$self->{tmp}->{is_table_alias}->{$table_name} )
        {
            return $self->do_err("Table '$table_name' referenced$location but not found in FROM list!");
        }
    }
    else
    {
        my @tblnamelist = ( keys( %{ $self->{tmp}->{is_table_name} } ), keys( %{ $self->{tmp}->{is_table_alias} } ) );
        my $tblnames = join( "|", @tblnamelist );
        unless ( $table_name =~ m/^(?:$tblnames)$/i )
        {
            return $self->do_err(
                "Table '$table_name' referenced$location but not found in FROM list (" . join( ",", @tblnamelist ) . ")!" );
        }
    }

    return 1;
}

###################################################################
# IDENTIFIER ::= <alphabetic_char> { <alphanumeric_char> | _ }...
#
# and must not be a reserved word or over 128 chars in length
###################################################################
sub IDENTIFIER
{
    my ( $self, $id ) = @_;
    if ( $id =~ m/^\?QI(.+)\?$/ or $id =~ m/^\?(.+)\?$/ )
    {
        return 1;
    }
    if ( $id =~ m/^[`](.+)[`]$/ )
    {
        $id = $1 and return 1;
    }
    if ( $id =~ m/^(.+)\.([^\.]+)$/ )
    {
        my $schema = $1;    # ignored
        $id = $2;
    }
    $id =~ s/\(|\)//g;
    return 1 if $id =~ m/^".+?"$/s;    # QUOTED IDENTIFIER
    my $err = "Bad table or column name: '$id' ";    # BAD CHARS
    if ( $id =~ /\W/ )
    {
        $err .= "has chars not alphanumeric or underscore!";
        return $self->do_err($err);
    }
    # CSV requires optional start with _
    my $badStartRx = uc( $self->{dialect} ) eq 'ANYDATA' ? qr/^\d/ : qr/^[_\d]/;
    if ( $id =~ $badStartRx )
    {                                                # BAD START
        $err .= "starts with non-alphabetic character!";
        return $self->do_err($err);
    }
    if ( length $id > 128 )
    {                                                # BAD LENGTH
        $err .= "contains more than 128 characters!";
        return $self->do_err($err);
    }
    $id = uc $id;
    if ( $self->{opts}->{reserved_words}->{$id} )
    {                                                # BAD RESERVED WORDS
        $err .= "is a SQL reserved word!";
        return $self->do_err($err);
    }
    return 1;
}

########################################
# PRIVATE METHODS AND UTILITY FUNCTIONS
########################################
sub order_joins
{
    my ( $self, $links ) = @_;
    for my $link (@$links)
    {
        if ( $link !~ /\./ )
        {
            return [];
        }
    }
    @$links = map { s/^(.+)\..*$/$1/; $1; } @$links;
    my @all_tables;
    my %relations;
    my %is_table;
    while (@$links)
    {
        my $t1 = shift @$links;
        my $t2 = shift @$links;
        return undef unless defined $t1 and defined $t2;
        push @all_tables, $t1 unless $is_table{$t1}++;
        push @all_tables, $t2 unless $is_table{$t2}++;
        $relations{$t1}{$t2}++;
        $relations{$t2}{$t1}++;
    }
    my @tables     = @all_tables;
    my @order      = shift @tables;
    my %is_ordered = ( $order[0] => 1 );
    my %visited;
    while (@tables)
    {
        my $t    = shift @tables;
        my @rels = keys %{ $relations{$t} };
        for my $t2 (@rels)
        {
            next unless $is_ordered{$t2};
            push @order, $t;
            $is_ordered{$t}++;
            last;
        }
        if ( !$is_ordered{$t} )
        {
            push @tables, $t if $visited{$t}++ < @all_tables;
        }
    }
    return $self->do_err("Unconnected tables in equijoin statement!")
      if @order < @all_tables;
    return \@order;
}

# PROVIDE BACKWARD COMPATIBILIT FOR JOCHEN'S FEATURE ATTRIBUTES TO NEW
#
#
sub set_feature_flags
{
    my ( $self, $select, $create ) = @_;
    if ( defined $select )
    {
        delete $self->{select};
        $self->{opts}->{valid_options}->{SELECT_MULTIPLE_TABLES} =
          $self->{opts}->{select}->{join} = $select->{join};
    }
    if ( defined $create )
    {
        delete $self->{create};
        for my $key ( keys %$create )
        {
            my $type = $key;
            $type =~ s/type_(.*)/\U$1/;
            $self->{opts}->{valid_data_types}->{$type} = $self->{opts}->{create}->{$key} =
              $create->{$key};
        }
    }
}

sub clean_sql
{
    my ( $self, $sql ) = @_;
    my $fields;
    my $i = -1;
    my $e = '\\';
    $e = quotemeta($e);

    #
    # patch from cpan@goess.org, adds support for col2=''
    #
    # $sql =~ s~'(([^'$e]|$e.|'')+)'~push(@$fields,$1);$i++;"?$i?"~ge;
    $sql =~ s~(?<!')'(([^'$e]|$e.|'')+)'~push(@$fields,$1);$i++;"?$i?"~ge;

    #
    foreach (@$fields) { $_ =~ s/''/\\'/g; }
    my @a = $sql =~ m/((?<!\\)(?:(?:\\\\)*)')/g;
    if ( ( scalar(@a) % 2 ) == 1 )
    {
        $sql =~ s/^.*\?(.+)$/$1/;
        $self->do_err("Mismatched single quote before: <$sql>");
    }
    if ( $sql =~ m/\?\?(\d)\?/ )
    {
        $sql = $fields->[$1];
        $self->do_err("Mismatched single quote: <$sql>");
    }
    foreach (@$fields) { $_ =~ s/$e'/'/g; s/^'(.*)'$/$1/; }

    #
    # From Steffen G. to correctly return newlines from $dbh->quote;
    #
    foreach (@$fields) { $_ =~ s/([^\\])\\r/$1\r/g; }
    foreach (@$fields) { $_ =~ s/([^\\])\\n/$1\n/g; }

    $self->{struct}->{literals} = $fields;

    my $qids;
    $i = -1;
    $e = q/""/;

    #    $sql =~ s~"(([^"$e]|$e.)+)"~push(@$qids,$1);$i++;"?QI$i?"~ge;
    $sql =~ s/"(([^"]|"")+)"/push(@$qids,$1);$i++;"?QI$i?"/ge;

    #@$qids = map { s/$e'/'/g; s/^'(.*)'$/$1/; $_} @$qids;
    $self->{struct}->{quoted_ids} = $qids if ($qids);

    #    $sql =~ s~'(([^'\\]|\\.)+)'~push(@$fields,$1);$i++;"?$i?"~ge;
    #    @$fields = map { s/\\'/'/g; s/^'(.*)'$/$1/; $_} @$fields;
    #print "$sql [@$fields]\n";# if $sql =~ /SELECT/;

## before line 1511
    my $comment_re = $self->{comment_re};

    #    if ( $sql =~ s/($comment_re)//gs) {
    #       $self->{comment} = $1;
    #    }
    if ( $sql =~ m/(.*)$comment_re$/s )
    {
        $sql = $1;
        $self->{comment} = $2;
    }
    if ( $sql =~ m/^(.*)--(.*)(\n|$)/ )
    {
        $sql = $1;
        $self->{comment} = $2;
    }

    $sql =~ s/\n/ /g;
    $sql =~ s/\s+/ /g;
    $sql =~ s/(\S)\(/$1 (/g;    # ensure whitespace before (
    $sql =~ s/\)(\S)/) $1/g;    # ensure whitespace after )
    $sql =~ s/\(\s*/(/g;        # trim whitespace after (
    $sql =~ s/\s*\)/)/g;        # trim whitespace before )
                                #
                                # $sql =~ s/\s*\(/(/g;   # trim whitespace before (
                                # $sql =~ s/\)\s*/)/g;   # trim whitespace after )
                                #    for my $op (qw(= <> < > <= >= \|\|))
                                #    {
                                #        $sql =~ s/(\S)$op/$1 $op/g;
                                #        $sql =~ s/$op(\S)/$op $1/g;
                                #    }
    $sql =~ s/(\S)([<>]?=|<>|<|>|\|\|)/$1 $2/g;
    $sql =~ s/([<>]?=|<>|<|>|\|\|)(\S)/$1 $2/g;
    $sql =~ s/< >/<>/g;
    $sql =~ s/< =/<=/g;
    $sql =~ s/> =/>=/g;
    $sql =~ s/\s*,/,/g;
    $sql =~ s/,\s*/,/g;
    $sql =~ s/^\s+//;
    $sql =~ s/\s+$//;

    return $sql;
}

sub trim
{
    my $str = $_[0] or return ('');
    $str =~ s/^\s+//;
    $str =~ s/\s+$//;
    return $str;
}

sub do_err
{
    my ( $self, $err, $errstr ) = @_;

    # $err = $errtype ? "DIALECT ERROR: $err" : "SQL ERROR: $err";
    $self->{struct}->{errstr} = $err;

    carp $err  if ( $self->{PrintError} );
    croak $err if ( $self->{RaiseError} );
    return;
}

#
#	DAA
#	abstract method so subclasses can provide
#	their own syntax transformations
#
sub transform_syntax
{
    my ( $self, $str ) = @_;
    return $str;
}

sub DESTROY
{
    my $self = $_[0];

    undef $self->{opts};
    undef $self->{struct};
    undef $self->{tmp};
    undef $self->{dialect};
    undef $self->{dialect_set};
}

1;

__END__

=pod

=head1 NAME

 SQL::Parser -- validate and parse SQL strings

=head1 SYNOPSIS

 use SQL::Parser;                                     # CREATE A PARSER OBJECT
 my $parser = SQL::Parser->new();

 $parser->feature( $class, $name, $value );           # SET OR FIND STATUS OF
 my $has_feature = $parser->feature( $class, $name ); # A PARSER FEATURE

 $parser->dialect( $dialect_name );                   # SET OR FIND STATUS OF
 my $current_dialect = $parser->dialect;              # A PARSER DIALECT


=head1 DESCRIPTION

SQL::Parser is part of the SQL::Statement distribution and, most
interaction with the parser should be done through SQL::Statement.
The methods shown above create and modify a parser object.  To use the
parser object to parse SQL and to examine the resulting structure, you
should use SQL::Statement.

B<Important Note>: Previously SQL::Parser had its own hash-based
interface for parsing, but that is now deprecated and will eventually
be phased out in favor of the object-oriented parsing interface of
SQL::Statement.  If you are unable to transition some features to the
new interface or have concerns about the phase out, please contact me.
See L<The Parse Structure> for details of the now-deprecated hash
method if you still need them.

=head1 METHODS

=head2 new()

Create a new parser object

 use SQL::Parser;
 my $parser = SQL::Parser->new();

The new() method creates a SQL::Parser object which can then be
used to parse and validate the syntax of SQL strings. It takes two
optional parameters - 1) the name of the SQL dialect that will define
the syntax rules for the parser and 2) a reference to a hash which can
contain additional attributes of the parser.  If no dialect is specified,
'AnyData' is the default.

 use SQL::Parser;
 my $parser = SQL::Parser->new( $dialect_name, \%attrs );

The dialect_name parameter is a string containing any valid
dialect such as 'ANSI', 'AnyData', or 'CSV'.  See the section on
the dialect() method below for details.

The C<attrs> parameter is a reference to a hash that can
contain error settings for the PrintError and RaiseError
attributes.

An example:

  use SQL::Parser;
  my $parser = SQL::Parser->new('AnyData', {RaiseError=>1} );

  This creates a new parser that uses the grammar rules
  contained in the .../SQL/Dialects/AnyData.pm file and which
  sets the RaiseError attribute to true.

=head2 dialect()

 $parser->dialect( $dialect_name );     # load a dialect configuration file
 my $dialect = $parser->dialect;        # get the name of the current dialect

 For example:

   $parser->dialect('AnyData');  # loads the AnyData config file
   print $parser->dialect;       # prints 'AnyData'

The C<$dialect_name> parameter may be the name of any dialect
configuration file on your system.  Use the
$parser->list('dialects') method to see a list of available
dialects.  At a minimum it will include "ANSI", "CSV", and
"AnyData".  For backwards compatibility 'Ansi' is accepted as a
synonym for 'ANSI', otherwise the names are case sensitive.

Loading a new dialect configuration file erases all current
parser features and resets them to those defined in the
configuration file.

=head2 feature()

Features define the rules to be used by a specific parser
instance.  They are divided into the following classes:

    * valid_commands
    * valid_options
    * valid_comparison_operators
    * valid_data_types
    * reserved_words

Within each class a feature name is either enabled or
disabled. For example, under "valid_data_types" the name "BLOB"
may be either disabled or enabled.  If it is not enabled
(either by being specifically disabled, or simply by not being
specified at all) then any SQL string using "BLOB" as a data
type will throw a syntax error "Invalid data type: 'BLOB'".

The feature() method allows you to enable, disable, or check the
status of any feature.

 $parser->feature( $class, $name, 1 );             # enable a feature

 $parser->feature( $class, $name, 0 );             # disable a feature

 my $feature = $parser->feature( $class, $name );  # return status of a feature

 For example:

 $parser->feature('reserved_words','FOO',1);       # make 'FOO' a reserved word

 $parser->feature('valid_data_types','BLOB',0);    # disallow 'BLOB' as a
                                                   # data type

                                                   # determine if the LIKE
                                                   # operator is supported
 my $LIKE = $parser->feature('valid_comparison_operators','LIKE');

See the section below on "Backwards Compatibility" for use of
the feature() method with SQL::Statement 0.1x style parameters.

=begin undocumented

=head2 clean_sql

=head2 command

=head2 create_op_regexen

=head2 do_err

=head2 errstr

=head2 extract_column_list

=head2 extract_func_args

=head2 group_ands

=head2 is_func

=head2 list

=head2 non_parens_search

=head2 nongroup_numeric

=head2 nongroup_string

=head2 order_joins

=head2 parens_search

=head2 parse

=head2 repl_btwin

=head2 replace_quoted

=head2 replace_quoted_commas

=head2 replace_quoted_ids

=head2 set_feature_flags

=head2 structure

=head2 transform_concat

=head2 trim

=head2 transform_syntax

=head2 undo_math_funcs

=head2 undo_string_funcs

=end undocumented

=head1 Supported SQL syntax

The SQL::Statement distribution can be used to either just parse SQL
statements or to execute them against actual data.  A broader set of
syntax is supported in the parser than in the executor.  For example
the parser allows you to specify column constraints like PRIMARY KEY.
Currently, these are ignored by the execution engine.  Likewise syntax
such as RESTRICT and CASCADE on DROP statements or LOCAL GLOBAL TEMPORARY
tables in CREATE are supported by the parser but ignored by the executor.

To see the list of Supported SQL syntax formerly kept in this pod, see
L<SQL::Statement>.

=head1 Subclassing SQL::Parser

In the event you need to either extend or modify SQL::Parser's
default behavior, the following methods may be overridden:

=over

=item C<$self->E<gt>C<get_btwn($string)>

Processes the BETWEEN...AND... predicates; default converts to
2 range predicates.

=item C<$self->E<gt>C<get_in($string)>

Process the IN (...list...) predicates; default converts to
a series of OR'd '=' predicate, or AND'd '<>' predicates for
NOT IN.

=item C<$self->E<gt>C<transform_syntax($string)>

Abstract method; default simply returns the original string.
Called after repl_btwn() and repl_in(), but before any further
predicate processing is applied. Possible uses include converting
other predicate syntax not recognized by SQL::Parser into user-defined
functions.

=back

=head1 The parse structure

This section outlines the B<now-deprecated> hash interface to the
parsed structure.  It is included B<for backwards compatibility only>.
You should use the SQL::Statement object interface to the structure
instead.  See L<SQL::Statement>.

B<Parse Structures>

Here are some further examples of the data structures returned
by the structure() method after a call to parse().  Only
specific details are shown for each SQL instance, not the entire
structure.

B<parse()>

Once a SQL::Parser object has been created with the new()
method, the parse() method can be used to parse any number of
SQL strings.  It takes a single required parameter -- a string
containing a SQL command.  The SQL string may optionally be
terminated by a semicolon.  The parse() method returns a true
value if the parse is successful and a false value if the parse
finds SQL syntax errors.

Examples:

  1) my $success = $parser->parse('SELECT * FROM foo');

  2) my $sql = 'SELECT * FROM foo';
     my $success = $parser->parse( $sql );

  3) my $success = $parser->parse(qq!
         SELECT id,phrase
           FROM foo
          WHERE id < 7
            AND phrase <> 'bar'
       ORDER BY phrase;
   !);

  4) my $success = $parser->parse('SELECT * FRoOM foo ');

In examples #1,#2, and #3, the value of $success will be true
because the strings passed to the parse() method are valid SQL
strings.

In example #4, however, the value of $success will be false
because the string contains a SQL syntax error ('FRoOM' instead
of 'FROM').

In addition to checking the return value of parse() with a
variable like $success, you may use the PrintError and
RaiseError attributes as you would in a DBI script:

 * If PrintError is true, then SQL syntax errors will be sent as
   warnings to STDERR (i.e. to the screen or to a file if STDERR
   has been redirected).  This is set to true by default which
   means that unless you specifically turn it off, all errors
   will be reported.

 * If RaiseError is true, then SQL syntax errors will cause the
   script to die, (i.e. the script will terminate unless wrapped
   in an eval).  This is set to false by default which means
   that unless you specifically turn it on, scripts will
   continue to operate even if there are SQL syntax errors.

Basically, you should leave PrintError on or else you will not
be warned when an error occurs.  If you are simply validating a
series of strings, you will want to leave RaiseError off so that
the script can check all strings regardless of whether some of
them contain SQL errors.  However, if you are going to try to
execute the SQL or need to depend that it is correct, you should
set RaiseError on so that the program will only continue to
operate if all SQL strings use correct syntax.

IMPORTANT NOTE #1: The parse() method only checks syntax, it
does NOT verify if the objects listed actually exist.  For
example, given the string "SELECT model FROM cars", the parse()
method will report that the string contains valid SQL but that
will not tell you whether there actually is a table called
"cars" or whether that table contains a column called 'model'.
Those kinds of verifications are performed by the
SQL::Statement module, not by SQL::Parser by itself.

IMPORTANT NOTE #2: The parse() method uses rules as defined by
the selected dialect configuration file and the feature()
method.  This means that a statement that is valid in one
dialect may not be valid in another.  For example the 'CSV' and
'AnyData' dialects define 'BLOB' as a valid data type but the
'ANSI' dialect does not.  Therefore the statement 'CREATE TABLE
foo (picture BLOB)' would be valid in the first two dialects but
would produce a syntax error in the 'ANSI' dialect.

B<structure()>

After a SQL::Parser object has been created and the parse()
method used to parse a SQL string, the structure() method
returns the data structure of that string.  This data structure
may be passed on to other modules (e.g. SQL::Statement) or it
may be printed out using, for example, the Data::Dumper module.

The data structure contains all of the information in the SQL
string as parsed into its various components.  To take a simple
example:

 $parser->parse('SELECT make,model FROM cars');
 use Data::Dumper;
 print Dumper $parser->structure;

Would produce:

 $VAR1 = {
          'column_defs' => [
                              { 'type'  => 'column',
                                'value' => 'make', },
                              { 'type'  => 'column',
                                'value' => 'model', },
                            ],
          'command' => 'SELECT',
          'table_names' => [
                             'cars'
                           ]
        };


 'SELECT make,model, FROM cars'

      command => 'SELECT',
      table_names => [ 'cars' ],
      column_names => [ 'make', 'model' ],

 'CREATE TABLE cars ( id INTEGER, model VARCHAR(40) )'

      column_defs => {
          id    => { data_type => INTEGER     },
          model => { data_type => VARCHAR(40) },
      },

 'SELECT DISTINCT make FROM cars'

      set_quantifier => 'DISTINCT',

 'SELECT MAX (model) FROM cars'

    set_function   => {
        name => 'MAX',
        arg  => 'models',
    },

 'SELECT * FROM cars LIMIT 5,10'

    limit_clause => {
        offset => 5,
        limit  => 10,
    },

 'SELECT * FROM vars ORDER BY make, model DESC'

    sort_spec_list => [
        { make  => 'ASC'  },
        { model => 'DESC' },
    ],

 "INSERT INTO cars VALUES ( 7, 'Chevy', 'Impala' )"

    values => [ 7, 'Chevy', 'Impala' ],

=head1 SUPPORT

You can find documentation for this module with the perldoc command.

    perldoc SQL::Parser
    perldoc SQL::Statement

You can also look for information at:

=over 4

=item * RT: CPAN's request tracker

L<http://rt.cpan.org/NoAuth/Bugs.html?Dist=SQL-Statement>

=item * AnnoCPAN: Annotated CPAN documentation

L<http://annocpan.org/dist/SQL-Statement>

=item * CPAN Ratings

L<http://cpanratings.perl.org/s/SQL-Statement>

=item * Search CPAN

L<http://search.cpan.org/dist/SQL-Statement/>

=back

=head2 Where can I go for help?

For questions about installation or usage, please ask on the
dbi-users@perl.org mailing list or post a question on PerlMonks
(L<http://www.perlmonks.org/>, where Jeff is known as jZed).
Jens does not visit PerlMonks on a regular basis.

If you have a bug report, a patch or a suggestion, please open a new
report ticket at CPAN (but please check previous reports first in case
your issue has already been addressed). You can mail any of the module
maintainers, but you are more assured of an answer by posting to the
dbi-users list or reporting the issue in RT.

Report tickets should contain a detailed description of the bug or
enhancement request and at least an easily verifiable way of
reproducing the issue or fix. Patches are always welcome, too.

=head2 Where can I go for help with a concrete version?

Bugs and feature requests are accepted against the latest version
only. To get patches for earlier versions, you need to get an
agreement with a developer of your choice - who may or not report the
the issue and a suggested fix upstream (depends on the license you
have chosen).

=head2 Business support and maintenance

For business support you can contact Jens via his CPAN email
address rehsackATcpan.org. Please keep in mind that business
support is neither available for free nor are you eligible to
receive any support based on the license distributed with this
package.


=head1 AUTHOR & COPYRIGHT

 This module is

 copyright (c) 2001,2005 by Jeff Zucker and
 copyright (c) 2007-2017 by Jens Rehsack.

 All rights reserved.

The module may be freely distributed under the same terms as
Perl itself using either the "GPL License" or the "Artistic
License" as specified in the Perl README file.

Jeff can be reached at: jzuckerATcpan.org
Jens can be reached at: rehsackATcpan.org or via dbi-devATperl.org

=cut
