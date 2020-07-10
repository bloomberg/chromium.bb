#!/usr/bin/perl

use strict;
use warnings;
require 5.008_001; # just as DBI

package DBD::mysql;

use DBI;
use DynaLoader();
use Carp;
our @ISA = qw(DynaLoader);

# please make sure the sub-version does not increase above '099'
# SQL_DRIVER_VER is formatted as dd.dd.dddd
# for version 5.x please switch to 5.00(_00) version numbering
# keep $VERSION in Bundle/DBD/mysql.pm in sync
our $VERSION = '4.050';

bootstrap DBD::mysql $VERSION;


our $err = 0;	    # holds error code for DBI::err
our $errstr = "";	# holds error string for DBI::errstr
our $drh = undef;	# holds driver handle once initialised

my $methods_are_installed = 0;
sub driver{
    return $drh if $drh;
    my($class, $attr) = @_;

    $class .= "::dr";

    # not a 'my' since we use it above to prevent multiple drivers
    $drh = DBI::_new_drh($class, { 'Name' => 'mysql',
				   'Version' => $VERSION,
				   'Err'    => \$DBD::mysql::err,
				   'Errstr' => \$DBD::mysql::errstr,
				   'Attribution' => 'DBD::mysql by Patrick Galbraith'
				 });

    if (!$methods_are_installed) {
	DBD::mysql::db->install_method('mysql_fd');
	DBD::mysql::db->install_method('mysql_async_result');
	DBD::mysql::db->install_method('mysql_async_ready');
	DBD::mysql::st->install_method('mysql_async_result');
	DBD::mysql::st->install_method('mysql_async_ready');

	$methods_are_installed++;
    }

    $drh;
}

sub CLONE {
  undef $drh;
}

sub _OdbcParse($$$) {
    my($class, $dsn, $hash, $args) = @_;
    my($var, $val);
    if (!defined($dsn)) {
	return;
    }
    while (length($dsn)) {
	if ($dsn =~ /([^:;]*\[.*]|[^:;]*)[:;](.*)/) {
	    $val = $1;
	    $dsn = $2;
	    $val =~ s/\[|]//g; # Remove [] if present, the rest of the code prefers plain IPv6 addresses
	} else {
	    $val = $dsn;
	    $dsn = '';
	}
	if ($val =~ /([^=]*)=(.*)/) {
	    $var = $1;
	    $val = $2;
	    if ($var eq 'hostname'  ||  $var eq 'host') {
		$hash->{'host'} = $val;
	    } elsif ($var eq 'db'  ||  $var eq 'dbname') {
		$hash->{'database'} = $val;
	    } else {
		$hash->{$var} = $val;
	    }
	} else {
	    foreach $var (@$args) {
		if (!defined($hash->{$var})) {
		    $hash->{$var} = $val;
		    last;
		}
	    }
	}
    }
}

sub _OdbcParseHost ($$) {
    my($class, $dsn) = @_;
    my($hash) = {};
    $class->_OdbcParse($dsn, $hash, ['host', 'port']);
    ($hash->{'host'}, $hash->{'port'});
}

sub AUTOLOAD {
    my ($meth) = $DBD::mysql::AUTOLOAD;
    my ($smeth) = $meth;
    $smeth =~ s/(.*)\:\://;

    my $val = constant($smeth, @_ ? $_[0] : 0);
    if ($! == 0) { eval "sub $meth { $val }"; return $val; }

    Carp::croak "$meth: Not defined";
}

1;


package DBD::mysql::dr; # ====== DRIVER ======
use strict;
use DBI qw(:sql_types);
use DBI::Const::GetInfoType;

sub connect {
    my($drh, $dsn, $username, $password, $attrhash) = @_;
    my($port);
    my($cWarn);
    my $connect_ref= { 'Name' => $dsn };
    my $dbi_imp_data;

    # Avoid warnings for undefined values
    $username ||= '';
    $password ||= '';
    $attrhash ||= {};
    $attrhash->{mysql_conn_attrs} ||= {};
    $attrhash->{mysql_conn_attrs}->{'program_name'} ||= $0;

    # create a 'blank' dbh
    my($this, $privateAttrHash) = (undef, $attrhash);
    $privateAttrHash = { %$privateAttrHash,
	'Name' => $dsn,
	'user' => $username,
	'password' => $password
    };

    DBD::mysql->_OdbcParse($dsn, $privateAttrHash,
				    ['database', 'host', 'port']);


    $dbi_imp_data = delete $attrhash->{dbi_imp_data};
    $connect_ref->{'dbi_imp_data'} = $dbi_imp_data;

    if (!defined($this = DBI::_new_dbh($drh,
            $connect_ref,
            $privateAttrHash)))
    {
      return undef;
    }

    DBD::mysql::db::_login($this, $dsn, $username, $password)
	  or $this = undef;

    if ($this && ($ENV{MOD_PERL} || $ENV{GATEWAY_INTERFACE})) {
        $this->{mysql_auto_reconnect} = 1;
    }
    $this;
}

sub data_sources {
    my($self) = shift;
    my($attributes) = shift;
    my($host, $port, $user, $password) = ('', '', '', '');
    if ($attributes) {
      $host = $attributes->{host} || '';
      $port = $attributes->{port} || '';
      $user = $attributes->{user} || '';
      $password = $attributes->{password} || '';
    }
    my(@dsn) = $self->func($host, $port, $user, $password, '_ListDBs');
    my($i);
    for ($i = 0;  $i < @dsn;  $i++) {
	$dsn[$i] = "DBI:mysql:$dsn[$i]";
    }
    @dsn;
}

sub admin {
    my($drh) = shift;
    my($command) = shift;
    my($dbname) = ($command eq 'createdb'  ||  $command eq 'dropdb') ?
	shift : '';
    my($host, $port) = DBD::mysql->_OdbcParseHost(shift(@_) || '');
    my($user) = shift || '';
    my($password) = shift || '';

    $drh->func(undef, $command,
	       $dbname || '',
	       $host || '',
	       $port || '',
	       $user, $password, '_admin_internal');
}

package DBD::mysql::db; # ====== DATABASE ======
use strict;
use DBI qw(:sql_types);

%DBD::mysql::db::db2ANSI = (
    "INT"   =>  "INTEGER",
    "CHAR"  =>  "CHAR",
    "REAL"  =>  "REAL",
    "IDENT" =>  "DECIMAL"
);

### ANSI datatype mapping to MySQL datatypes
%DBD::mysql::db::ANSI2db = (
    "CHAR"          => "CHAR",
    "VARCHAR"       => "CHAR",
    "LONGVARCHAR"   => "CHAR",
    "NUMERIC"       => "INTEGER",
    "DECIMAL"       => "INTEGER",
    "BIT"           => "INTEGER",
    "TINYINT"       => "INTEGER",
    "SMALLINT"      => "INTEGER",
    "INTEGER"       => "INTEGER",
    "BIGINT"        => "INTEGER",
    "REAL"          => "REAL",
    "FLOAT"         => "REAL",
    "DOUBLE"        => "REAL",
    "BINARY"        => "CHAR",
    "VARBINARY"     => "CHAR",
    "LONGVARBINARY" => "CHAR",
    "DATE"          => "CHAR",
    "TIME"          => "CHAR",
    "TIMESTAMP"     => "CHAR"
);

sub prepare {
    my($dbh, $statement, $attribs)= @_;

    return unless $dbh->func('_async_check');

    # create a 'blank' dbh
    my $sth = DBI::_new_sth($dbh, {'Statement' => $statement});

    # Populate internal handle data.
    if (!DBD::mysql::st::_prepare($sth, $statement, $attribs)) {
	$sth = undef;
    }

    $sth;
}

sub db2ANSI {
    my $self = shift;
    my $type = shift;
    return $DBD::mysql::db::db2ANSI{"$type"};
}

sub ANSI2db {
    my $self = shift;
    my $type = shift;
    return $DBD::mysql::db::ANSI2db{"$type"};
}

sub admin {
    my($dbh) = shift;
    my($command) = shift;
    my($dbname) = ($command eq 'createdb'  ||  $command eq 'dropdb') ?
	shift : '';
    $dbh->{'Driver'}->func($dbh, $command, $dbname, '', '', '',
			   '_admin_internal');
}

sub _SelectDB ($$) {
    die "_SelectDB is removed from this module; use DBI->connect instead.";
}

sub table_info ($) {
  my ($dbh, $catalog, $schema, $table, $type, $attr) = @_;
  $dbh->{mysql_server_prepare}||= 0;
  my $mysql_server_prepare_save= $dbh->{mysql_server_prepare};
  $dbh->{mysql_server_prepare}= 0;
  my @names = qw(TABLE_CAT TABLE_SCHEM TABLE_NAME TABLE_TYPE REMARKS);
  my @rows;

  my $sponge = DBI->connect("DBI:Sponge:", '','')
    or return $dbh->DBI::set_err($DBI::err, "DBI::Sponge: $DBI::errstr");

# Return the list of catalogs
  if (defined $catalog && $catalog eq "%" &&
      (!defined($schema) || $schema eq "") &&
      (!defined($table) || $table eq ""))
  {
    @rows = (); # Empty, because MySQL doesn't support catalogs (yet)
  }
  # Return the list of schemas
  elsif (defined $schema && $schema eq "%" &&
      (!defined($catalog) || $catalog eq "") &&
      (!defined($table) || $table eq ""))
  {
    my $sth = $dbh->prepare("SHOW DATABASES")
      or ($dbh->{mysql_server_prepare}= $mysql_server_prepare_save &&
          return undef);

    $sth->execute()
      or ($dbh->{mysql_server_prepare}= $mysql_server_prepare_save &&
        return DBI::set_err($dbh, $sth->err(), $sth->errstr()));

    while (my $ref = $sth->fetchrow_arrayref())
    {
      push(@rows, [ undef, $ref->[0], undef, undef, undef ]);
    }
  }
  # Return the list of table types
  elsif (defined $type && $type eq "%" &&
      (!defined($catalog) || $catalog eq "") &&
      (!defined($schema) || $schema eq "") &&
      (!defined($table) || $table eq ""))
  {
    @rows = (
        [ undef, undef, undef, "TABLE", undef ],
        [ undef, undef, undef, "VIEW",  undef ],
        );
  }
  # Special case: a catalog other than undef, "", or "%"
  elsif (defined $catalog && $catalog ne "" && $catalog ne "%")
  {
    @rows = (); # Nothing, because MySQL doesn't support catalogs yet.
  }
  # Uh oh, we actually have a meaty table_info call. Work is required!
  else
  {
    my @schemas;
    # If no table was specified, we want them all
    $table ||= "%";

    # If something was given for the schema, we need to expand it to
    # a list of schemas, since it may be a wildcard.
    if (defined $schema && $schema ne "")
    {
      my $sth = $dbh->prepare("SHOW DATABASES LIKE " .
          $dbh->quote($schema))
        or ($dbh->{mysql_server_prepare}= $mysql_server_prepare_save &&
        return undef);
      $sth->execute()
        or ($dbh->{mysql_server_prepare}= $mysql_server_prepare_save &&
        return DBI::set_err($dbh, $sth->err(), $sth->errstr()));

      while (my $ref = $sth->fetchrow_arrayref())
      {
        push @schemas, $ref->[0];
      }
    }
    # Otherwise we want the current database
    else
    {
      push @schemas, $dbh->selectrow_array("SELECT DATABASE()");
    }

    # Figure out which table types are desired
    my ($want_tables, $want_views);
    if (defined $type && $type ne "")
    {
      $want_tables = ($type =~ m/table/i);
      $want_views  = ($type =~ m/view/i);
    }
    else
    {
      $want_tables = $want_views = 1;
    }

    for my $database (@schemas)
    {
      my $sth = $dbh->prepare("SHOW /*!50002 FULL*/ TABLES FROM " .
          $dbh->quote_identifier($database) .
          " LIKE " .  $dbh->quote($table))
          or ($dbh->{mysql_server_prepare}= $mysql_server_prepare_save &&
          return undef);

      $sth->execute() or
          ($dbh->{mysql_server_prepare}= $mysql_server_prepare_save &&
          return DBI::set_err($dbh, $sth->err(), $sth->errstr()));

      while (my $ref = $sth->fetchrow_arrayref())
      {
        my $type = (defined $ref->[1] &&
            $ref->[1] =~ /view/i) ? 'VIEW' : 'TABLE';
        next if $type eq 'TABLE' && not $want_tables;
        next if $type eq 'VIEW'  && not $want_views;
        push @rows, [ undef, $database, $ref->[0], $type, undef ];
      }
    }
  }

  my $sth = $sponge->prepare("table_info",
  {
    rows          => \@rows,
    NUM_OF_FIELDS => scalar @names,
    NAME          => \@names,
  })
    or ($dbh->{mysql_server_prepare}= $mysql_server_prepare_save &&
      return $dbh->DBI::set_err($sponge->err(), $sponge->errstr()));

  $dbh->{mysql_server_prepare}= $mysql_server_prepare_save;
  return $sth;
}

sub _ListTables {
  my $dbh = shift;
  if (!$DBD::mysql::QUIET) {
    warn "_ListTables is deprecated, use \$dbh->tables()";
  }
  return map { $_ =~ s/.*\.//; $_ } $dbh->tables();
}


sub column_info {
  my ($dbh, $catalog, $schema, $table, $column) = @_;

  return unless $dbh->func('_async_check');

  $dbh->{mysql_server_prepare}||= 0;
  my $mysql_server_prepare_save= $dbh->{mysql_server_prepare};
  $dbh->{mysql_server_prepare}= 0;

  # ODBC allows a NULL to mean all columns, so we'll accept undef
  $column = '%' unless defined $column;

  my $ER_NO_SUCH_TABLE= 1146;

  my $table_id = $dbh->quote_identifier($catalog, $schema, $table);

  my @names = qw(
      TABLE_CAT TABLE_SCHEM TABLE_NAME COLUMN_NAME
      DATA_TYPE TYPE_NAME COLUMN_SIZE BUFFER_LENGTH DECIMAL_DIGITS
      NUM_PREC_RADIX NULLABLE REMARKS COLUMN_DEF
      SQL_DATA_TYPE SQL_DATETIME_SUB CHAR_OCTET_LENGTH
      ORDINAL_POSITION IS_NULLABLE CHAR_SET_CAT
      CHAR_SET_SCHEM CHAR_SET_NAME COLLATION_CAT COLLATION_SCHEM COLLATION_NAME
      UDT_CAT UDT_SCHEM UDT_NAME DOMAIN_CAT DOMAIN_SCHEM DOMAIN_NAME
      SCOPE_CAT SCOPE_SCHEM SCOPE_NAME MAX_CARDINALITY
      DTD_IDENTIFIER IS_SELF_REF
      mysql_is_pri_key mysql_type_name mysql_values
      mysql_is_auto_increment
      );
  my %col_info;

  local $dbh->{FetchHashKeyName} = 'NAME_lc';
  # only ignore ER_NO_SUCH_TABLE in internal_execute if issued from here
  my $desc_sth = $dbh->prepare("DESCRIBE $table_id " . $dbh->quote($column));
  my $desc = $dbh->selectall_arrayref($desc_sth, { Columns=>{} });

  #return $desc_sth if $desc_sth->err();
  if (my $err = $desc_sth->err())
  {
    # return the error, unless it is due to the table not
    # existing per DBI spec
    if ($err != $ER_NO_SUCH_TABLE)
    {
      $dbh->{mysql_server_prepare}= $mysql_server_prepare_save;
      return undef;
    }
    $dbh->set_err(undef,undef);
    $desc = [];
  }

  my $ordinal_pos = 0;
  my @fields;
  for my $row (@$desc)
  {
    my $type = $row->{type};
    $type =~ m/^(\w+)(\((.+)\))?\s?(.*)?$/;
    my $basetype  = lc($1);
    my $typemod   = $3;
    my $attr      = $4;

    push @fields, $row->{field};
    my $info = $col_info{ $row->{field} }= {
	    TABLE_CAT               => $catalog,
	    TABLE_SCHEM             => $schema,
	    TABLE_NAME              => $table,
	    COLUMN_NAME             => $row->{field},
	    NULLABLE                => ($row->{null} eq 'YES') ? 1 : 0,
	    IS_NULLABLE             => ($row->{null} eq 'YES') ? "YES" : "NO",
	    TYPE_NAME               => uc($basetype),
	    COLUMN_DEF              => $row->{default},
	    ORDINAL_POSITION        => ++$ordinal_pos,
	    mysql_is_pri_key        => ($row->{key}  eq 'PRI'),
	    mysql_type_name         => $row->{type},
      mysql_is_auto_increment => ($row->{extra} =~ /auto_increment/i ? 1 : 0),
    };
    #
	  # This code won't deal with a pathological case where a value
	  # contains a single quote followed by a comma, and doesn't unescape
	  # any escaped values. But who would use those in an enum or set?
    #
	  my @type_params= ($typemod && index($typemod,"'")>=0) ?
      ("$typemod," =~ /'(.*?)',/g)  # assume all are quoted
			: split /,/, $typemod||'';      # no quotes, plain list
	  s/''/'/g for @type_params;                # undo doubling of quotes

	  my @type_attr= split / /, $attr||'';

  	$info->{DATA_TYPE}= SQL_VARCHAR();
    if ($basetype =~ /^(char|varchar|\w*text|\w*blob)/)
    {
      $info->{DATA_TYPE}= SQL_CHAR() if $basetype eq 'char';
      if ($type_params[0])
      {
        $info->{COLUMN_SIZE} = $type_params[0];
      }
      else
      {
        $info->{COLUMN_SIZE} = 65535;
        $info->{COLUMN_SIZE} = 255        if $basetype =~ /^tiny/;
        $info->{COLUMN_SIZE} = 16777215   if $basetype =~ /^medium/;
        $info->{COLUMN_SIZE} = 4294967295 if $basetype =~ /^long/;
      }
    }
	  elsif ($basetype =~ /^(binary|varbinary)/)
    {
      $info->{COLUMN_SIZE} = $type_params[0];
	    # SQL_BINARY & SQL_VARBINARY are tempting here but don't match the
	    # semantics for mysql (not hex). SQL_CHAR &  SQL_VARCHAR are correct here.
	    $info->{DATA_TYPE} = ($basetype eq 'binary') ? SQL_CHAR() : SQL_VARCHAR();
    }
    elsif ($basetype =~ /^(enum|set)/)
    {
	    if ($basetype eq 'set')
      {
		    $info->{COLUMN_SIZE} = length(join ",", @type_params);
	    }
	    else
      {
        my $max_len = 0;
        length($_) > $max_len and $max_len = length($_) for @type_params;
        $info->{COLUMN_SIZE} = $max_len;
	    }
	    $info->{"mysql_values"} = \@type_params;
    }
    elsif ($basetype =~ /int/ || $basetype eq 'bit' )
    {
      # big/medium/small/tiny etc + unsigned?
	    $info->{DATA_TYPE} = SQL_INTEGER();
	    $info->{NUM_PREC_RADIX} = 10;
	    $info->{COLUMN_SIZE} = $type_params[0];
    }
    elsif ($basetype =~ /^decimal/)
    {
      $info->{DATA_TYPE} = SQL_DECIMAL();
      $info->{NUM_PREC_RADIX} = 10;
      $info->{COLUMN_SIZE}    = $type_params[0];
      $info->{DECIMAL_DIGITS} = $type_params[1];
    }
    elsif ($basetype =~ /^(float|double)/)
    {
	    $info->{DATA_TYPE} = ($basetype eq 'float') ? SQL_FLOAT() : SQL_DOUBLE();
	    $info->{NUM_PREC_RADIX} = 2;
	    $info->{COLUMN_SIZE} = ($basetype eq 'float') ? 32 : 64;
    }
    elsif ($basetype =~ /date|time/)
    {
      # date/datetime/time/timestamp
	    if ($basetype eq 'time' or $basetype eq 'date')
      {
		    #$info->{DATA_TYPE}   = ($basetype eq 'time') ? SQL_TYPE_TIME() : SQL_TYPE_DATE();
        $info->{DATA_TYPE}   = ($basetype eq 'time') ? SQL_TIME() : SQL_DATE();
        $info->{COLUMN_SIZE} = ($basetype eq 'time') ? 8 : 10;
      }
	    else
      {
        # datetime/timestamp
        #$info->{DATA_TYPE}     = SQL_TYPE_TIMESTAMP();
		    $info->{DATA_TYPE}        = SQL_TIMESTAMP();
		    $info->{SQL_DATA_TYPE}    = SQL_DATETIME();
        $info->{SQL_DATETIME_SUB} = $info->{DATA_TYPE} - ($info->{SQL_DATA_TYPE} * 10);
        $info->{COLUMN_SIZE}      = ($basetype eq 'datetime') ? 19 : $type_params[0] || 14;
	    }
	    $info->{DECIMAL_DIGITS}= 0; # no fractional seconds
    }
    elsif ($basetype eq 'year')
    {
      # no close standard so treat as int
	    $info->{DATA_TYPE}      = SQL_INTEGER();
	    $info->{NUM_PREC_RADIX} = 10;
	    $info->{COLUMN_SIZE}    = 4;
	  }
	  else
    {
	    Carp::carp("column_info: unrecognized column type '$basetype' of $table_id.$row->{field} treated as varchar");
    }
    $info->{SQL_DATA_TYPE} ||= $info->{DATA_TYPE};
    #warn Dumper($info);
  }

  my $sponge = DBI->connect("DBI:Sponge:", '','')
    or (  $dbh->{mysql_server_prepare}= $mysql_server_prepare_save &&
          return $dbh->DBI::set_err($DBI::err, "DBI::Sponge: $DBI::errstr"));

  my $sth = $sponge->prepare("column_info $table", {
      rows          => [ map { [ @{$_}{@names} ] } map { $col_info{$_} } @fields ],
      NUM_OF_FIELDS => scalar @names,
      NAME          => \@names,
      }) or
  return ($dbh->{mysql_server_prepare}= $mysql_server_prepare_save &&
          $dbh->DBI::set_err($sponge->err(), $sponge->errstr()));

  $dbh->{mysql_server_prepare}= $mysql_server_prepare_save;
  return $sth;
}


sub primary_key_info {
  my ($dbh, $catalog, $schema, $table) = @_;

  return unless $dbh->func('_async_check');

  $dbh->{mysql_server_prepare}||= 0;
  my $mysql_server_prepare_save= $dbh->{mysql_server_prepare};

  my $table_id = $dbh->quote_identifier($catalog, $schema, $table);

  my @names = qw(
      TABLE_CAT TABLE_SCHEM TABLE_NAME COLUMN_NAME KEY_SEQ PK_NAME
      );
  my %col_info;

  local $dbh->{FetchHashKeyName} = 'NAME_lc';
  my $desc_sth = $dbh->prepare("SHOW KEYS FROM $table_id");
  my $desc= $dbh->selectall_arrayref($desc_sth, { Columns=>{} });
  my $ordinal_pos = 0;
  for my $row (grep { $_->{key_name} eq 'PRIMARY'} @$desc)
  {
    $col_info{ $row->{column_name} }= {
      TABLE_CAT   => $catalog,
      TABLE_SCHEM => $schema,
      TABLE_NAME  => $table,
      COLUMN_NAME => $row->{column_name},
      KEY_SEQ     => $row->{seq_in_index},
      PK_NAME     => $row->{key_name},
    };
  }

  my $sponge = DBI->connect("DBI:Sponge:", '','')
    or
     ($dbh->{mysql_server_prepare}= $mysql_server_prepare_save &&
      return $dbh->DBI::set_err($DBI::err, "DBI::Sponge: $DBI::errstr"));

  my $sth= $sponge->prepare("primary_key_info $table", {
      rows          => [
        map { [ @{$_}{@names} ] }
        sort { $a->{KEY_SEQ} <=> $b->{KEY_SEQ} }
        values %col_info
      ],
      NUM_OF_FIELDS => scalar @names,
      NAME          => \@names,
      }) or
       ($dbh->{mysql_server_prepare}= $mysql_server_prepare_save &&
        return $dbh->DBI::set_err($sponge->err(), $sponge->errstr()));

  $dbh->{mysql_server_prepare}= $mysql_server_prepare_save;

  return $sth;
}


sub foreign_key_info {
    my ($dbh,
        $pk_catalog, $pk_schema, $pk_table,
        $fk_catalog, $fk_schema, $fk_table,
       ) = @_;

    return unless $dbh->func('_async_check');

    # INFORMATION_SCHEMA.KEY_COLUMN_USAGE was added in 5.0.6
    # no one is going to be running 5.0.6, taking out the check for $point > .6
    my ($maj, $min, $point) = _version($dbh);
    return if $maj < 5 ;

    my $sql = <<'EOF';
SELECT NULL AS PKTABLE_CAT,
       A.REFERENCED_TABLE_SCHEMA AS PKTABLE_SCHEM,
       A.REFERENCED_TABLE_NAME AS PKTABLE_NAME,
       A.REFERENCED_COLUMN_NAME AS PKCOLUMN_NAME,
       A.TABLE_CATALOG AS FKTABLE_CAT,
       A.TABLE_SCHEMA AS FKTABLE_SCHEM,
       A.TABLE_NAME AS FKTABLE_NAME,
       A.COLUMN_NAME AS FKCOLUMN_NAME,
       A.ORDINAL_POSITION AS KEY_SEQ,
       NULL AS UPDATE_RULE,
       NULL AS DELETE_RULE,
       A.CONSTRAINT_NAME AS FK_NAME,
       NULL AS PK_NAME,
       NULL AS DEFERABILITY,
       NULL AS UNIQUE_OR_PRIMARY
  FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE A,
       INFORMATION_SCHEMA.TABLE_CONSTRAINTS B
 WHERE A.TABLE_SCHEMA = B.TABLE_SCHEMA AND A.TABLE_NAME = B.TABLE_NAME
   AND A.CONSTRAINT_NAME = B.CONSTRAINT_NAME AND B.CONSTRAINT_TYPE IS NOT NULL
EOF

    my @where;
    my @bind;

    # catalogs are not yet supported by MySQL

#    if (defined $pk_catalog) {
#        push @where, 'A.REFERENCED_TABLE_CATALOG = ?';
#        push @bind, $pk_catalog;
#    }

    if (defined $pk_schema) {
        push @where, 'A.REFERENCED_TABLE_SCHEMA = ?';
        push @bind, $pk_schema;
    }

    if (defined $pk_table) {
        push @where, 'A.REFERENCED_TABLE_NAME = ?';
        push @bind, $pk_table;
    }

#    if (defined $fk_catalog) {
#        push @where, 'A.TABLE_CATALOG = ?';
#        push @bind,  $fk_schema;
#    }

    if (defined $fk_schema) {
        push @where, 'A.TABLE_SCHEMA = ?';
        push @bind,  $fk_schema;
    }

    if (defined $fk_table) {
        push @where, 'A.TABLE_NAME = ?';
        push @bind,  $fk_table;
    }

    if (@where) {
        $sql .= ' AND ';
        $sql .= join ' AND ', @where;
    }
    $sql .= " ORDER BY A.TABLE_SCHEMA, A.TABLE_NAME, A.ORDINAL_POSITION";

    local $dbh->{FetchHashKeyName} = 'NAME_uc';
    my $sth = $dbh->prepare($sql);
    $sth->execute(@bind);

    return $sth;
}
# #86030: PATCH: adding statistics_info support
# Thank you to David Dick http://search.cpan.org/~ddick/
sub statistics_info {
    my ($dbh,
        $catalog, $schema, $table,
        $unique_only, $quick,
       ) = @_;

    return unless $dbh->func('_async_check');

    # INFORMATION_SCHEMA.KEY_COLUMN_USAGE was added in 5.0.6
    # no one is going to be running 5.0.6, taking out the check for $point > .6
    my ($maj, $min, $point) = _version($dbh);
    return if $maj < 5 ;

    my $sql = <<'EOF';
SELECT TABLE_CATALOG AS TABLE_CAT,
       TABLE_SCHEMA AS TABLE_SCHEM,
       TABLE_NAME AS TABLE_NAME,
       NON_UNIQUE AS NON_UNIQUE,
       NULL AS INDEX_QUALIFIER,
       INDEX_NAME AS INDEX_NAME,
       LCASE(INDEX_TYPE) AS TYPE,
       SEQ_IN_INDEX AS ORDINAL_POSITION,
       COLUMN_NAME AS COLUMN_NAME,
       COLLATION AS ASC_OR_DESC,
       CARDINALITY AS CARDINALITY,
       NULL AS PAGES,
       NULL AS FILTER_CONDITION
  FROM INFORMATION_SCHEMA.STATISTICS
EOF

    my @where;
    my @bind;

    # catalogs are not yet supported by MySQL

#    if (defined $catalog) {
#        push @where, 'TABLE_CATALOG = ?';
#        push @bind, $catalog;
#    }

    if (defined $schema) {
        push @where, 'TABLE_SCHEMA = ?';
        push @bind, $schema;
    }

    if (defined $table) {
        push @where, 'TABLE_NAME = ?';
        push @bind, $table;
    }

    if (@where) {
        $sql .= ' WHERE ';
        $sql .= join ' AND ', @where;
    }
    $sql .= " ORDER BY TABLE_SCHEMA, TABLE_NAME, ORDINAL_POSITION";

    local $dbh->{FetchHashKeyName} = 'NAME_uc';
    my $sth = $dbh->prepare($sql);
    $sth->execute(@bind);

    return $sth;
}

sub _version {
    my $dbh = shift;

    return
        $dbh->get_info($DBI::Const::GetInfoType::GetInfoType{SQL_DBMS_VER})
            =~ /(\d+)\.(\d+)\.(\d+)/;
}


####################
# get_info()
# Generated by DBI::DBD::Metadata

sub get_info {
    my($dbh, $info_type) = @_;

    return unless $dbh->func('_async_check');
    require DBD::mysql::GetInfo;
    my $v = $DBD::mysql::GetInfo::info{int($info_type)};
    $v = $v->($dbh) if ref $v eq 'CODE';
    return $v;
}

BEGIN {
    my @needs_async_check = qw/data_sources quote_identifier begin_work/;

    foreach my $method (@needs_async_check) {
        no strict 'refs';

        my $super = "SUPER::$method";
        *$method  = sub {
            my $h = shift;
            return unless $h->func('_async_check');
            return $h->$super(@_);
        };
    }
}


package DBD::mysql::st; # ====== STATEMENT ======
use strict;

BEGIN {
    my @needs_async_result = qw/fetchrow_hashref fetchall_hashref/;
    my @needs_async_check = qw/bind_param_array bind_col bind_columns execute_for_fetch/;

    foreach my $method (@needs_async_result) {
        no strict 'refs';

        my $super = "SUPER::$method";
        *$method = sub {
            my $sth = shift;
            if(defined $sth->mysql_async_ready) {
                return unless $sth->mysql_async_result;
            }
            return $sth->$super(@_);
        };
    }

    foreach my $method (@needs_async_check) {
        no strict 'refs';

        my $super = "SUPER::$method";
        *$method = sub {
            my $h = shift;
            return unless $h->func('_async_check');
            return $h->$super(@_);
        };
    }
}

1;

__END__

=pod

=encoding utf8

=head1 NAME

DBD::mysql - MySQL driver for the Perl5 Database Interface (DBI)

=head1 SYNOPSIS

    use DBI;

    my $dsn = "DBI:mysql:database=$database;host=$hostname;port=$port";
    my $dbh = DBI->connect($dsn, $user, $password);

    my $sth = $dbh->prepare(
        'SELECT id, first_name, last_name FROM authors WHERE last_name = ?')
        or die "prepare statement failed: $dbh->errstr()";
    $sth->execute('Eggers') or die "execution failed: $dbh->errstr()";
    print $sth->rows . " rows found.\n";
    while (my $ref = $sth->fetchrow_hashref()) {
        print "Found a row: id = $ref->{'id'}, fn = $ref->{'first_name'}\n";
    }
    $sth->finish;


=head1 EXAMPLE

  #!/usr/bin/perl

  use strict;
  use warnings;
  use DBI;

  # Connect to the database.
  my $dbh = DBI->connect("DBI:mysql:database=test;host=localhost",
                         "joe", "joe's password",
                         {'RaiseError' => 1});

  # Drop table 'foo'. This may fail, if 'foo' doesn't exist
  # Thus we put an eval around it.
  eval { $dbh->do("DROP TABLE foo") };
  print "Dropping foo failed: $@\n" if $@;

  # Create a new table 'foo'. This must not fail, thus we don't
  # catch errors.
  $dbh->do("CREATE TABLE foo (id INTEGER, name VARCHAR(20))");

  # INSERT some data into 'foo'. We are using $dbh->quote() for
  # quoting the name.
  $dbh->do("INSERT INTO foo VALUES (1, " . $dbh->quote("Tim") . ")");

  # same thing, but using placeholders (recommended!)
  $dbh->do("INSERT INTO foo VALUES (?, ?)", undef, 2, "Jochen");

  # now retrieve data from the table.
  my $sth = $dbh->prepare("SELECT * FROM foo");
  $sth->execute();
  while (my $ref = $sth->fetchrow_hashref()) {
    print "Found a row: id = $ref->{'id'}, name = $ref->{'name'}\n";
  }
  $sth->finish();

  # Disconnect from the database.
  $dbh->disconnect();


=head1 DESCRIPTION

B<DBD::mysql> is the Perl5 Database Interface driver for the MySQL
database. In other words: DBD::mysql is an interface between the Perl
programming language and the MySQL programming API that comes with
the MySQL relational database management system. Most functions
provided by this programming API are supported. Some rarely used
functions are missing, mainly because no-one ever requested
them. :-)

In what follows we first discuss the use of DBD::mysql,
because this is what you will need the most. For installation, see the
separate document L<DBD::mysql::INSTALL>.
See L</"EXAMPLE"> for a simple example above.

From perl you activate the interface with the statement

  use DBI;

After that you can connect to multiple MySQL database servers
and send multiple queries to any of them via a simple object oriented
interface. Two types of objects are available: database handles and
statement handles. Perl returns a database handle to the connect
method like so:

  $dbh = DBI->connect("DBI:mysql:database=$db;host=$host",
    $user, $password, {RaiseError => 1});

Once you have connected to a database, you can execute SQL
statements with:

  my $query = sprintf("INSERT INTO foo VALUES (%d, %s)",
		      $number, $dbh->quote("name"));
  $dbh->do($query);

See L<DBI> for details on the quote and do methods. An alternative
approach is

  $dbh->do("INSERT INTO foo VALUES (?, ?)", undef,
	   $number, $name);

in which case the quote method is executed automatically. See also
the bind_param method in L<DBI>. See L</"DATABASE HANDLES"> below
for more details on database handles.

If you want to retrieve results, you need to create a so-called
statement handle with:

  $sth = $dbh->prepare("SELECT * FROM $table");
  $sth->execute();

This statement handle can be used for multiple things. First of all
you can retrieve a row of data:

  my $row = $sth->fetchrow_hashref();

If your table has columns ID and NAME, then $row will be hash ref with
keys ID and NAME. See L</"STATEMENT HANDLES"> below for more details on
statement handles.

But now for a more formal approach:


=head2 Class Methods

=over

=item B<connect>

    use DBI;

    $dsn = "DBI:mysql:$database";
    $dsn = "DBI:mysql:database=$database;host=$hostname";
    $dsn = "DBI:mysql:database=$database;host=$hostname;port=$port";

    $dbh = DBI->connect($dsn, $user, $password);

The C<database> is not a required attribute, but please note that MySQL
has no such thing as a default database. If you don't specify the database
at connection time your active database will be null and you'd need to prefix
your tables with the database name; i.e. 'SELECT * FROM mydb.mytable'.

This is similar to the behavior of the mysql command line client. Also,
'SELECT DATABASE()' will return the current database active for the handle.

=over

=item host

=item port

The hostname, if not specified or specified as '' or 'localhost', will
default to a MySQL server running on the local machine using the default for
the UNIX socket. To connect to a MySQL server on the local machine via TCP,
you must specify the loopback IP address (127.0.0.1) as the host.

Should the MySQL server be running on a non-standard port number,
you may explicitly state the port number to connect to in the C<hostname>
argument, by concatenating the I<hostname> and I<port number> together
separated by a colon ( C<:> ) character or by using the  C<port> argument.

To connect to a MySQL server on localhost using TCP/IP, you must specify the
hostname as 127.0.0.1 (with the optional port).

When connecting to a MySQL Server with IPv6, a bracketed IPv6 address should be used.
Example DSN:

  my $dsn = "DBI:mysql:;host=[1a12:2800:6f2:85::f20:8cf];port=3306";


=item mysql_client_found_rows

Enables (TRUE value) or disables (FALSE value) the flag CLIENT_FOUND_ROWS
while connecting to the MySQL server. This has a somewhat funny effect:
Without mysql_client_found_rows, if you perform a query like

  UPDATE $table SET id = 1 WHERE id = 1;

then the MySQL engine will always return 0, because no rows have changed.
With mysql_client_found_rows however, it will return the number of rows
that have an id 1, as some people are expecting. (At least for compatibility
to other engines.)

=item mysql_compression

If your DSN contains the option "mysql_compression=1", then the communication
between client and server will be compressed.

=item mysql_connect_timeout

If your DSN contains the option "mysql_connect_timeout=##", the connect
request to the server will timeout if it has not been successful after
the given number of seconds.

=item mysql_write_timeout

If your DSN contains the option "mysql_write_timeout=##", the write
operation to the server will timeout if it has not been successful after
the given number of seconds.

=item mysql_read_timeout

If your DSN contains the option "mysql_read_timeout=##", the read
operation to the server will timeout if it has not been successful after
the given number of seconds.

=item mysql_init_command

If your DSN contains the option "mysql_init_command=##", then
this SQL statement is executed when connecting to the MySQL server.
It is automatically re-executed if reconnection occurs.

=item mysql_skip_secure_auth

This option is for older mysql databases that don't have secure auth set.

=item mysql_read_default_file

=item mysql_read_default_group

These options can be used to read a config file like /etc/my.cnf or
~/.my.cnf. By default MySQL's C client library doesn't use any config
files unlike the client programs (mysql, mysqladmin, ...) that do, but
outside of the C client library. Thus you need to explicitly request
reading a config file, as in

    $dsn = "DBI:mysql:test;mysql_read_default_file=/home/joe/my.cnf";
    $dbh = DBI->connect($dsn, $user, $password)

The option mysql_read_default_group can be used to specify the default
group in the config file: Usually this is the I<client> group, but
see the following example:

    [client]
    host=localhost

    [perl]
    host=perlhost

(Note the order of the entries! The example won't work, if you reverse
the [client] and [perl] sections!)

If you read this config file, then you'll be typically connected to
I<localhost>. However, by using

    $dsn = "DBI:mysql:test;mysql_read_default_group=perl;"
        . "mysql_read_default_file=/home/joe/my.cnf";
    $dbh = DBI->connect($dsn, $user, $password);

you'll be connected to I<perlhost>. Note that if you specify a
default group and do not specify a file, then the default config
files will all be read.  See the documentation of
the C function mysql_options() for details.

=item mysql_socket

It is possible to choose the Unix socket that is
used for connecting to the server. This is done, for example, with

    mysql_socket=/dev/mysql

Usually there's no need for this option, unless you are using another
location for the socket than that built into the client.

=item mysql_ssl

A true value turns on the CLIENT_SSL flag when connecting to the MySQL
server and enforce SSL encryption.  A false value (which is default)
disable SSL encryption with the MySQL server.

When enabling SSL encryption you should set also other SSL options,
at least mysql_ssl_ca_file or mysql_ssl_ca_path.

  mysql_ssl=1 mysql_ssl_verify_server_cert=1 mysql_ssl_ca_file=/path/to/ca_cert.pem

This means that your communication with the server will be encrypted.

Please note that this can only work if you enabled SSL when compiling
DBD::mysql; this is the default starting version 4.034.
See L<DBD::mysql::INSTALL> for more details.

=item mysql_ssl_ca_file

The path to a file in PEM format that contains a list of trusted SSL
certificate authorities.

When set MySQL server certificate is checked that it is signed by some
CA certificate in the list.  Common Name value is not verified unless
C<mysql_ssl_verify_server_cert> is enabled.

=item mysql_ssl_ca_path

The path to a directory that contains trusted SSL certificate authority
certificates in PEM format.

When set MySQL server certificate is checked that it is signed by some
CA certificate in the list.  Common Name value is not verified unless
C<mysql_ssl_verify_server_cert> is enabled.

Please note that this option is supported only if your MySQL client was
compiled with OpenSSL library, and not with default yaSSL library.

=item mysql_ssl_verify_server_cert

Checks the server's Common Name value in the certificate that the server
sends to the client.  The client verifies that name against the host name
the client uses for connecting to the server, and the connection fails if
there is a mismatch.  For encrypted connections, this option helps prevent
man-in-the-middle attacks.

Verification of the host name is disabled by default.

=item mysql_ssl_client_key

The name of the SSL key file in PEM format to use for establishing
a secure connection.

=item mysql_ssl_client_cert

The name of the SSL certificate file in PEM format to use for
establishing a secure connection.

=item mysql_ssl_cipher

A list of permissible ciphers to use for connection encryption.  If no
cipher in the list is supported, encrypted connections will not work.

  mysql_ssl_cipher=AES128-SHA
  mysql_ssl_cipher=DHE-RSA-AES256-SHA:AES128-SHA

=item mysql_ssl_optional

Setting C<mysql_ssl_optional> to true disables strict SSL enforcement
and makes SSL connection optional.  This option opens security hole
for man-in-the-middle attacks.  Default value is false which means
that C<mysql_ssl> set to true enforce SSL encryption.

This option was introduced in 4.043 version of DBD::mysql.  Due to
L<The BACKRONYM|http://backronym.fail/> and L<The Riddle|http://riddle.link/>
vulnerabilities in libmysqlclient library, enforcement of SSL
encryption was not possbile and therefore C<mysql_ssl_optional=1>
was effectively set for all DBD::mysql versions prior to 4.043.
Starting with 4.043, DBD::mysql with C<mysql_ssl=1> could refuse
connection to MySQL server if underlaying libmysqlclient library is
vulnerable.  Option C<mysql_ssl_optional> can be used to make SSL
connection vulnerable.

=item mysql_server_pubkey

Path to the RSA public key of the server. This is used for the
sha256_password and caching_sha2_password authentication plugins.

=item mysql_get_server_pubkey

Setting C<mysql_get_server_pubkey> to true requests the public
RSA key of the server.

=item mysql_local_infile

The LOCAL capability for LOAD DATA may be disabled
in the MySQL client library by default. If your DSN contains the option
"mysql_local_infile=1", LOAD DATA LOCAL will be enabled.  (However,
this option is *ineffective* if the server has also been configured to
disallow LOCAL.)

=item mysql_multi_statements

Support for multiple statements separated by a semicolon
(;) may be enabled by using this option. Enabling this option may cause
problems if server-side prepared statements are also enabled.

=item mysql_server_prepare

This option is used to enable server side prepared statements.

To use server side prepared statements, all you need to do is set the variable
mysql_server_prepare in the connect:

  $dbh = DBI->connect(
    "DBI:mysql:database=test;host=localhost;mysql_server_prepare=1",
    "",
    "",
    { RaiseError => 1, AutoCommit => 1 }
  );

or:

  $dbh = DBI->connect(
    "DBI:mysql:database=test;host=localhost",
    "",
    "",
    { RaiseError => 1, AutoCommit => 1, mysql_server_prepare => 1 }
  );

There are many benefits to using server side prepare statements, mostly if you are
performing many inserts because of that fact that a single statement is prepared
to accept multiple insert values.

To make sure that the 'make test' step tests whether server prepare works, you just
need to export the env variable MYSQL_SERVER_PREPARE:

  export MYSQL_SERVER_PREPARE=1

Please note that mysql server cannot prepare or execute some prepared statements.
In this case DBD::mysql fallbacks to normal non-prepared statement and tries again.

=item mysql_server_prepare_disable_fallback

This option disable fallback to normal non-prepared statement when mysql server
does not support execution of current statement as prepared.

Useful when you want to be sure that statement is going to be executed as
server side prepared. Error message and code in case of failure is propagated
back to DBI.

=item mysql_embedded_options

The option <mysql_embedded_options> can be used to pass 'command-line'
options to embedded server.

Example:

  use DBI;
  $testdsn="DBI:mysqlEmb:database=test;mysql_embedded_options=--help,--verbose";
  $dbh = DBI->connect($testdsn,"a","b");

This would cause the command line help to the embedded MySQL server library
to be printed.


=item mysql_embedded_groups

The option <mysql_embedded_groups> can be used to specify the groups in the
config file(I<my.cnf>) which will be used to get options for embedded server.
If not specified [server] and [embedded] groups will be used.

Example:

  $testdsn="DBI:mysqlEmb:database=test;mysql_embedded_groups=embedded_server,common";

=item mysql_conn_attrs

The option <mysql_conn_attrs> is a hash of attribute names and values which can be
used to send custom connection attributes to the server. Some attributes like
'_os', '_platform', '_client_name' and '_client_version' are added by libmysqlclient
and 'program_name' is added by DBD::mysql.

You can then later read these attributes from the performance schema tables which
can be quite helpful for profiling your database or creating statistics.
You'll have to use a MySQL 5.6 server and libmysqlclient or newer to leverage this
feature.

  my $dbh= DBI->connect($dsn, $user, $password,
    { AutoCommit => 0,
      mysql_conn_attrs => {
        foo => 'bar',
        wiz => 'bang'
      },
    });

Now you can select the results from the performance schema tables. You can do this
in the same session, but also afterwards. It can be very useful to answer questions
like 'which script sent this query?'.

  my $results = $dbh->selectall_hashref(
    'SELECT * FROM performance_schema.session_connect_attrs',
    'ATTR_NAME'
  );

This returns:

  $result = {
    'foo' => {
        'ATTR_VALUE'       => 'bar',
        'PROCESSLIST_ID'   => '3',
        'ATTR_NAME'        => 'foo',
        'ORDINAL_POSITION' => '6'
    },
    'wiz' => {
        'ATTR_VALUE'       => 'bang',
        'PROCESSLIST_ID'   => '3',
        'ATTR_NAME'        => 'wiz',
        'ORDINAL_POSITION' => '3'
    },
    'program_name' => {
        'ATTR_VALUE'       => './foo.pl',
        'PROCESSLIST_ID'   => '3',
        'ATTR_NAME'        => 'program_name',
        'ORDINAL_POSITION' => '5'
    },
    '_client_name' => {
        'ATTR_VALUE'       => 'libmysql',
        'PROCESSLIST_ID'   => '3',
        'ATTR_NAME'        => '_client_name',
        'ORDINAL_POSITION' => '1'
    },
    '_client_version' => {
        'ATTR_VALUE'       => '5.6.24',
        'PROCESSLIST_ID'   => '3',
        'ATTR_NAME'        => '_client_version',
        'ORDINAL_POSITION' => '7'
    },
    '_os' => {
        'ATTR_VALUE'       => 'osx10.8',
        'PROCESSLIST_ID'   => '3',
        'ATTR_NAME'        => '_os',
        'ORDINAL_POSITION' => '0'
    },
    '_pid' => {
        'ATTR_VALUE'       => '59860',
        'PROCESSLIST_ID'   => '3',
        'ATTR_NAME'        => '_pid',
        'ORDINAL_POSITION' => '2'
    },
    '_platform' => {
        'ATTR_VALUE'       => 'x86_64',
        'PROCESSLIST_ID'   => '3',
        'ATTR_NAME'        => '_platform',
        'ORDINAL_POSITION' => '4'
    }
  };

=back

=back


=head2 Private MetaData Methods

=over

=item B<ListDBs>

    my $drh = DBI->install_driver("mysql");
    @dbs = $drh->func("$hostname:$port", '_ListDBs');
    @dbs = $drh->func($hostname, $port, '_ListDBs');
    @dbs = $dbh->func('_ListDBs');

Returns a list of all databases managed by the MySQL server
running on C<$hostname>, port C<$port>. This is a legacy
method.  Instead, you should use the portable method

    @dbs = DBI->data_sources("mysql");

=back


=head1 DATABASE HANDLES

The DBD::mysql driver supports the following attributes of database
handles (read only):

  $errno = $dbh->{'mysql_errno'};
  $error = $dbh->{'mysql_error'};
  $info = $dbh->{'mysql_hostinfo'};
  $info = $dbh->{'mysql_info'};
  $insertid = $dbh->{'mysql_insertid'};
  $info = $dbh->{'mysql_protoinfo'};
  $info = $dbh->{'mysql_serverinfo'};
  $info = $dbh->{'mysql_stat'};
  $threadId = $dbh->{'mysql_thread_id'};

These correspond to mysql_errno(), mysql_error(), mysql_get_host_info(),
mysql_info(), mysql_insert_id(), mysql_get_proto_info(),
mysql_get_server_info(), mysql_stat() and mysql_thread_id(),
respectively.

=over 2

=item mysql_clientinfo

List information of the MySQL client library that DBD::mysql was built
against:

  print "$dbh->{mysql_clientinfo}\n";

  5.2.0-MariaDB

=item mysql_clientversion

  print "$dbh->{mysql_clientversion}\n";

  50200

=item mysql_serverversion

  print "$dbh->{mysql_serverversion}\n";

  50200

=item mysql_dbd_stats

  $info_hashref = $dbh->{mysql_dbd_stats};

DBD::mysql keeps track of some statistics in the mysql_dbd_stats attribute.
The following stats are being maintained:

=over 8

=item auto_reconnects_ok

The number of times that DBD::mysql successfully reconnected to the mysql
server.

=item auto_reconnects_failed

The number of times that DBD::mysql tried to reconnect to mysql but failed.

=back

=back

The DBD::mysql driver also supports the following attributes of database
handles (read/write):

=over

=item mysql_auto_reconnect

This attribute determines whether DBD::mysql will automatically reconnect
to mysql if the connection be lost. This feature defaults to off; however,
if either the GATEWAY_INTERFACE or MOD_PERL environment variable is set,
DBD::mysql will turn mysql_auto_reconnect on.  Setting mysql_auto_reconnect
to on is not advised if 'lock tables' is used because if DBD::mysql reconnect
to mysql all table locks will be lost.  This attribute is ignored when
AutoCommit is turned off, and when AutoCommit is turned off, DBD::mysql will
not automatically reconnect to the server.

It is also possible to set the default value of the C<mysql_auto_reconnect>
attribute for the $dbh by passing it in the C<\%attr> hash for C<DBI->connect>.

  $dbh->{mysql_auto_reconnect} = 1;

or

  my $dbh = DBI->connect($dsn, $user, $password, {
     mysql_auto_reconnect => 1,
  });

Note that if you are using a module or framework that performs reconnections
for you (for example L<DBIx::Connector> in fixup mode), this value must be set
to 0.

=item mysql_use_result

This attribute forces the driver to use mysql_use_result rather than
mysql_store_result.  The former is faster and less memory consuming, but
tends to block other processes.  mysql_store_result is the default due to that
fact storing the result is expected behavior with most applications.

It is possible to set the default value of the C<mysql_use_result> attribute
for the $dbh via the DSN:

  $dbh = DBI->connect("DBI:mysql:test;mysql_use_result=1", "root", "");

You can also set it after creation of the database handle:

   $dbh->{mysql_use_result} = 0; # disable
   $dbh->{mysql_use_result} = 1; # enable

You can also set or unset the C<mysql_use_result> setting on your statement
handle, when creating the statement handle or after it has been created.
See L</"STATEMENT HANDLES">.

=item mysql_enable_utf8

This attribute determines whether DBD::mysql should assume strings
stored in the database are utf8.  This feature defaults to off.

When set, a data retrieved from a textual column type (char, varchar,
etc) will have the UTF-8 flag turned on if necessary.  This enables
character semantics on that string.  You will also need to ensure that
your database / table / column is configured to use UTF8. See for more
information the chapter on character set support in the MySQL manual:
L<http://dev.mysql.com/doc/refman/5.7/en/charset.html>

Additionally, turning on this flag tells MySQL that incoming data should
be treated as UTF-8.  This will only take effect if used as part of the
call to connect().  If you turn the flag on after connecting, you will
need to issue the command C<SET NAMES utf8> to get the same effect.

=item mysql_enable_utf8mb4

This is similar to mysql_enable_utf8, but is capable of handling 4-byte
UTF-8 characters.

=item mysql_bind_type_guessing

This attribute causes the driver (emulated prepare statements)
to attempt to guess if a value being bound is a numeric value,
and if so, doesn't quote the value.  This was created by
Dragonchild and is one way to deal with the performance issue
of using quotes in a statement that is inserting or updating a
large numeric value. This was previously called
C<unsafe_bind_type_guessing> because it is experimental. I have
successfully run the full test suite with this option turned on,
the name can now be simply C<mysql_bind_type_guessing>.

CAVEAT: Even though you can insert an integer value into a
character column, if this column is indexed, if you query that
column with the integer value not being quoted, it will not
use the index:

    MariaDB [test]> explain select * from test where value0 = '3' \G
    *************************** 1. row ***************************
               id: 1
      select_type: SIMPLE
            table: test
             type: ref
    possible_keys: value0
              key: value0
          key_len: 13
              ref: const
             rows: 1
            Extra: Using index condition
    1 row in set (0.00 sec)

    MariaDB [test]> explain select * from test where value0 = 3
        -> \G
    *************************** 1. row ***************************
               id: 1
      select_type: SIMPLE
            table: test
             type: ALL
    possible_keys: value0
              key: NULL
          key_len: NULL
              ref: NULL
             rows: 6
            Extra: Using where
    1 row in set (0.00 sec)

See bug: https://rt.cpan.org/Ticket/Display.html?id=43822

C<mysql_bind_type_guessing> can be turned on via

 - through DSN

  my $dbh= DBI->connect('DBI:mysql:test', 'username', 'pass',
  { mysql_bind_type_guessing => 1})

  - OR after handle creation

  $dbh->{mysql_bind_type_guessing} = 1;

=item mysql_bind_comment_placeholders

This attribute causes the driver (emulated prepare statements)
will cause any placeholders in comments to be bound. This is
not correct prepared statement behavior, but some developers
have come to depend on this behavior, so I have made it available
in 4.015

=item mysql_no_autocommit_cmd

This attribute causes the driver to not issue 'set autocommit'
either through explicit or using mysql_autocommit(). This is
particularly useful in the case of using MySQL Proxy.

See the bug report:

https://rt.cpan.org/Public/Bug/Display.html?id=46308


C<mysql_no_autocommit_cmd> can be turned on when creating the database
handle:

  my $dbh = DBI->connect('DBI:mysql:test', 'username', 'pass',
  { mysql_no_autocommit_cmd => 1});

or using an existing database handle:

  $dbh->{mysql_no_autocommit_cmd} = 1;

=item ping

This can be used to send a ping to the server.

  $rc = $dbh->ping();

=back


=head1 STATEMENT HANDLES

The statement handles of DBD::mysql support a number
of attributes. You access these by using, for example,

  my $numFields = $sth->{NUM_OF_FIELDS};

Note, that most attributes are valid only after a successful I<execute>.
An C<undef> value will returned otherwise. The most important exception
is the C<mysql_use_result> attribute, which forces the driver to use
mysql_use_result rather than mysql_store_result. The former is faster
and less memory consuming, but tends to block other processes. (That's why
mysql_store_result is the default.)

To set the C<mysql_use_result> attribute, use either of the following:

  my $sth = $dbh->prepare("QUERY", { mysql_use_result => 1});

or

  my $sth = $dbh->prepare($sql);
  $sth->{mysql_use_result} = 1;

Column dependent attributes, for example I<NAME>, the column names,
are returned as a reference to an array. The array indices are
corresponding to the indices of the arrays returned by I<fetchrow>
and similar methods. For example the following code will print a
header of table names together with all rows:

  my $sth = $dbh->prepare("SELECT * FROM $table") ||
    die "Error:" . $dbh->errstr . "\n";

  $sth->execute ||  die "Error:" . $sth->errstr . "\n";

  my $names = $sth->{NAME};
  my $numFields = $sth->{'NUM_OF_FIELDS'} - 1;
  for my $i ( 0..$numFields ) {
      printf("%s%s", $i ? "," : "", $$names[$i]);
  }
  print "\n";
  while (my $ref = $sth->fetchrow_arrayref) {
      for my $i ( 0..$numFields ) {
      printf("%s%s", $i ? "," : "", $$ref[$i]);
      }
      print "\n";
  }

For portable applications you should restrict yourself to attributes with
capitalized or mixed case names. Lower case attribute names are private
to DBD::mysql. The attribute list includes:

=over

=item ChopBlanks

this attribute determines whether a I<fetchrow> will chop preceding
and trailing blanks off the column values. Chopping blanks does not
have impact on the I<max_length> attribute.

=item mysql_gtids

Returns GTID(s) if GTID session tracking is ensabled in the server via
session_track_gtids.

=item mysql_insertid

If the statement you executed performs an INSERT, and there is an AUTO_INCREMENT
column in the table you inserted in, this attribute holds the value stored into
the AUTO_INCREMENT column, if that value is automatically generated, by
storing NULL or 0 or was specified as an explicit value.

Typically, you'd access the value via $sth->{mysql_insertid}. The value can
also be accessed via $dbh->{mysql_insertid} but this can easily
produce incorrect results in case one database handle is shared.

=item mysql_is_blob

Reference to an array of boolean values; TRUE indicates, that the
respective column is a blob. This attribute is valid for MySQL only.

=item mysql_is_key

Reference to an array of boolean values; TRUE indicates, that the
respective column is a key. This is valid for MySQL only.

=item mysql_is_num

Reference to an array of boolean values; TRUE indicates, that the
respective column contains numeric values.

=item mysql_is_pri_key

Reference to an array of boolean values; TRUE indicates, that the
respective column is a primary key.

=item mysql_is_auto_increment

Reference to an array of boolean values; TRUE indicates that the
respective column is an AUTO_INCREMENT column.  This is only valid
for MySQL.

=item mysql_length

=item mysql_max_length

A reference to an array of maximum column sizes. The I<max_length> is
the maximum physically present in the result table, I<length> gives
the theoretically possible maximum. I<max_length> is valid for MySQL
only.

=item NAME

A reference to an array of column names.

=item NULLABLE

A reference to an array of boolean values; TRUE indicates that this column
may contain NULL's.

=item NUM_OF_FIELDS

Number of fields returned by a I<SELECT> or I<LISTFIELDS> statement.
You may use this for checking whether a statement returned a result:
A zero value indicates a non-SELECT statement like I<INSERT>,
I<DELETE> or I<UPDATE>.

=item mysql_table

A reference to an array of table names, useful in a I<JOIN> result.

=item TYPE

A reference to an array of column types. The engine's native column
types are mapped to portable types like DBI::SQL_INTEGER() or
DBI::SQL_VARCHAR(), as good as possible. Not all native types have
a meaningful equivalent, for example DBD::mysql::FIELD_TYPE_INTERVAL
is mapped to DBI::SQL_VARCHAR().
If you need the native column types, use I<mysql_type>. See below.

=item mysql_type

A reference to an array of MySQL's native column types, for example
DBD::mysql::FIELD_TYPE_SHORT() or DBD::mysql::FIELD_TYPE_STRING().
Use the I<TYPE> attribute, if you want portable types like
DBI::SQL_SMALLINT() or DBI::SQL_VARCHAR().

=item mysql_type_name

Similar to mysql, but type names and not numbers are returned.
Whenever possible, the ANSI SQL name is preferred.

=item mysql_warning_count

The number of warnings generated during execution of the SQL statement.
This attribute is available on both statement handles and database handles.

=back

=head1 TRANSACTION SUPPORT

The transaction support works as follows:

=over

=item *

By default AutoCommit mode is on, following the DBI specifications.

=item *

If you execute

  $dbh->{AutoCommit} = 0;

or

  $dbh->{AutoCommit} = 1;

then the driver will set the MySQL server variable autocommit to 0 or
1, respectively. Switching from 0 to 1 will also issue a COMMIT,
following the DBI specifications.

=item *

The methods

    $dbh->rollback();
    $dbh->commit();

will issue the commands ROLLBACK and COMMIT, respectively. A
ROLLBACK will also be issued if AutoCommit mode is off and the
database handles DESTROY method is called. Again, this is following
the DBI specifications.

=back

Given the above, you should note the following:

=over

=item *

You should never change the server variable autocommit manually,
unless you are ignoring DBI's transaction support.

=item *

Switching AutoCommit mode from on to off or vice versa may fail.
You should always check for errors when changing AutoCommit mode.
The suggested way of doing so is using the DBI flag RaiseError.
If you don't like RaiseError, you have to use code like the
following:

  $dbh->{AutoCommit} = 0;
  if ($dbh->{AutoCommit}) {
    # An error occurred!
  }

=item *

If you detect an error while changing the AutoCommit mode, you
should no longer use the database handle. In other words, you
should disconnect and reconnect again, because the transaction
mode is unpredictable. Alternatively you may verify the transaction
mode by checking the value of the server variable autocommit.
However, such behaviour isn't portable.

=item *

DBD::mysql has a "reconnect" feature that handles the so-called
MySQL "morning bug": If the server has disconnected, most probably
due to a timeout, then by default the driver will reconnect and
attempt to execute the same SQL statement again. However, this
behaviour is disabled when AutoCommit is off: Otherwise the
transaction state would be completely unpredictable after a
reconnect.

=item *

The "reconnect" feature of DBD::mysql can be toggled by using the
L<mysql_auto_reconnect> attribute. This behaviour should be turned off
in code that uses LOCK TABLE because if the database server time out
and DBD::mysql reconnect, table locks will be lost without any
indication of such loss.

=back

=head1 MULTIPLE RESULT SETS

DBD::mysql supports multiple result sets, thanks to Guy Harrison!

The basic usage of multiple result sets is

  do
  {
    while (@row = $sth->fetchrow_array())
    {
      do stuff;
    }
  } while ($sth->more_results)

An example would be:

  $dbh->do("drop procedure if exists someproc") or print $DBI::errstr;

  $dbh->do("create procedure someproc() deterministic
   begin
   declare a,b,c,d int;
   set a=1;
   set b=2;
   set c=3;
   set d=4;
   select a, b, c, d;
   select d, c, b, a;
   select b, a, c, d;
   select c, b, d, a;
  end") or print $DBI::errstr;

  $sth=$dbh->prepare('call someproc()') ||
  die $DBI::err.": ".$DBI::errstr;

  $sth->execute || die DBI::err.": ".$DBI::errstr; $rowset=0;
  do {
    print "\nRowset ".++$i."\n---------------------------------------\n\n";
    foreach $colno (0..$sth->{NUM_OF_FIELDS}-1) {
      print $sth->{NAME}->[$colno]."\t";
    }
    print "\n";
    while (@row= $sth->fetchrow_array())  {
      foreach $field (0..$#row) {
        print $row[$field]."\t";
      }
      print "\n";
    }
  } until (!$sth->more_results)

=head2 Issues with multiple result sets

Please be aware there could be issues if your result sets are "jagged",
meaning the number of columns of your results vary. Varying numbers of
columns could result in your script crashing.


=head1 MULTITHREADING

The multithreading capabilities of DBD::mysql depend completely
on the underlying C libraries. The modules are working with handle data
only, no global variables are accessed or (to the best of my knowledge)
thread unsafe functions are called. Thus DBD::mysql is believed
to be completely thread safe, if the C libraries are thread safe
and you don't share handles among threads.

The obvious question is: Are the C libraries thread safe?
In the case of MySQL the answer is yes, since MySQL 5.5 it is.


=head1 ASYNCHRONOUS QUERIES

You can make a single asynchronous query per MySQL connection; this allows
you to submit a long-running query to the server and have an event loop
inform you when it's ready.  An asynchronous query is started by either
setting the 'async' attribute to a true value in the L<DBI/do> method,
or in the L<DBI/prepare> method.  Statements created with 'async' set to
true in prepare always run their queries asynchronously when L<DBI/execute>
is called.  The driver also offers three additional methods:
C<mysql_async_result>, C<mysql_async_ready>, and C<mysql_fd>.
C<mysql_async_result> returns what do or execute would have; that is, the
number of rows affected.  C<mysql_async_ready> returns true if
C<mysql_async_result> will not block, and zero otherwise.  They both return
C<undef> if that handle was not created with 'async' set to true
or if an asynchronous query was not started yet.
C<mysql_fd> returns the file descriptor number for the MySQL connection; you
can use this in an event loop.

Here's an example of how to use the asynchronous query interface:

  use feature 'say';
  $dbh->do('SELECT SLEEP(10)', { async => 1 });
  until($dbh->mysql_async_ready) {
    say 'not ready yet!';
    sleep 1;
  }
  my $rows = $dbh->mysql_async_result;

=head1 INSTALLATION

See L<DBD::mysql::INSTALL>.

=head1 AUTHORS

Originally, there was a non-DBI driver, Mysql, which was much like
PHP drivers such as mysql and mysqli. The B<Mysql> module was
originally written by Andreas König <koenig@kulturbox.de> who still, to this
day, contributes patches to DBD::mysql. An emulated version of Mysql was
provided to DBD::mysql from Jochen Wiedmann, but eventually deprecated as it
was another bundle of code to maintain.

The first incarnation of DBD::mysql was developed by Alligator Descartes,
who was also aided and abetted by Gary Shea, Andreas König and
Tim Bunce.

The current incarnation of B<DBD::mysql> was written by Jochen Wiedmann,
then numerous changes and bug-fixes were added by Rudy Lippan. Next,
prepared statement support was added by Patrick Galbraith and
Alexy Stroganov (who also solely added embedded server
support).

For the past nine years DBD::mysql has been maintained by
Patrick Galbraith (I<patg@patg.net>), and recently with the great help of
Michiel Beijen (I<michiel.beijen@gmail.com>),  along with the entire community
of Perl developers who keep sending patches to help continue improving DBD::mysql


=head1 CONTRIBUTIONS

Anyone who desires to contribute to this project is encouraged to do so.
Currently, the source code for this project can be found at Github:

L<https://github.com/perl5-dbi/DBD-mysql/>

Either fork this repository and produce a branch with your changeset that
the maintainer can merge to his tree, or create a diff with git. The maintainer
is more than glad to take contributions from the community as
many features and fixes from DBD::mysql have come from the community.


=head1 COPYRIGHT

This module is

=over

=item *

Large Portions Copyright (c) 2004-2013 Patrick Galbraith

=item *

Large Portions Copyright (c) 2004-2006 Alexey Stroganov

=item *

Large Portions Copyright (c) 2003-2005 Rudolf Lippan

=item *

Large Portions Copyright (c) 1997-2003 Jochen Wiedmann, with code portions

=item *

Copyright (c)1994-1997 their original authors

=back


=head1 LICENSE

This module is released under the same license as Perl itself. See
L<http://www.perl.com/perl/misc/Artistic.html> for details.


=head1 MAILING LIST SUPPORT

This module is maintained and supported on a mailing list, dbi-users.

To subscribe to this list, send an email to

dbi-users-subscribe@perl.org

Mailing list archives are at

L<http://groups.google.com/group/perl.dbi.users?hl=en&lr=>


=head1 ADDITIONAL DBI INFORMATION

Additional information on the DBI project can be found on the World
Wide Web at the following URL:

L<http://dbi.perl.org>

where documentation, pointers to the mailing lists and mailing list
archives and pointers to the most current versions of the modules can
be used.

Information on the DBI interface itself can be gained by typing:

    perldoc DBI

Information on DBD::mysql specifically can be gained by typing:

    perldoc DBD::mysql

(this will display the document you're currently reading)


=head1 BUG REPORTING, ENHANCEMENT/FEATURE REQUESTS

Please report bugs, including all the information needed
such as DBD::mysql version, MySQL version, OS type/version, etc
to this link:

L<https://rt.cpan.org/Dist/Display.html?Name=DBD-mysql>

Note: until recently, MySQL/Sun/Oracle responded to bugs and assisted in
fixing bugs which many thanks should be given for their help!
This driver is outside the realm of the numerous components they support, and the
maintainer and community solely support DBD::mysql

=cut
