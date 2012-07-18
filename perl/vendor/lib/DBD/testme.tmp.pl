#!/usr/bin/env perl

BEGIN {
	use lib '.', 'blib/lib', 'blib/arch';
	system 'make';
}

use strict;
use warnings;
use DBI ':sql_types';
use utf8;
use Data::Dumper;
use YAML;
use DBD::Pg qw/:pg_types/;
use Data::Peek;

use vars qw/$sth $info $count $SQL/;

my $tracelevel = shift || 0;
$ENV{DBI_TRACE} = $tracelevel;

my $DSN = 'DBI:Pg:dbname=postgres';
my $dbh = DBI->connect($DSN, '', '', {AutoCommit=>0,RaiseError=>1,PrintError=>0})
  or die "Connection failed!\n";

my $me = $dbh->{Driver}{Name};
print "DBI is version $DBI::VERSION, I am $me, version of DBD::Pg is $DBD::Pg::VERSION\n";

utf8_print_test();

#memory_leak_test_bug_65734();

exit;


sub utf8_print_test {

	## Set things up
	$dbh->do('CREATE TEMPORARY TABLE ctest (c TEXT)');

	## Add some UTF-8 content
	$dbh->do("INSERT INTO ctest VALUES ('*JIHOMORAVSKÝ*')");
	$dbh->do("INSERT INTO ctest VALUES ('*Špindlerův Mlýn*')");

	## Pull data back out via execute/bind/fetch
	$SQL = 'SELECT c FROM ctest';

	my $result;

	for my $loop (1..4) {

		my $onoff = 'off';
		if ($loop == 1 or $loop==3) {
			$dbh->{pg_enable_utf8} = 0;
		}
		else {
			$dbh->{pg_enable_utf8} = 1;
			$onoff = 'on';
		}

		if ($loop>2) {
			binmode STDOUT, ':utf8';
		}

		$sth = $dbh->prepare($SQL);
		$sth->execute();
		$sth->bind_columns(\$result);
		while ($sth->fetch() ) {
			print DPeek $result;
			print "\n Print with pg_enable_utf8 $onoff: $result\n";
			warn " Warn with pg_enable_utf8 $onoff: $result\n\n";
			utf8::upgrade($result);
			print DPeek $result; print "\n\n";
		}
	}

} ## end of utf8_print_test

sub memory_leak_test_bug_65734 {

	## Memory leak when an array appears in the bind variables

	## Set things up
	$dbh->do('CREATE TEMPORARY TABLE tbl1 (id SERIAL PRIMARY KEY, val INTEGER[])');
	$dbh->do('CREATE TEMPORARY TABLE tbl2 (id SERIAL PRIMARY KEY, val INTEGER)');

	## Subroutine that performs the leaking action
	sub leakmaker1 {
		$dbh->do('INSERT INTO tbl1(val) VALUES (?)', undef, [123]);
	}

	## Control subroutine that does not leak
	sub leakmaker2 {
		$dbh->do('INSERT INTO tbl2(val) VALUES (?)', undef, 123);
	}

	leakcheck(\&leakmaker1,1000);

	exit;

} ## end of memory_leak_test_bug_65734


sub leakcheck {

	my $sub = shift;
	my $count = shift || 1000;
	my $maxsize = shift || 100000;

	## Safety check:
	if (exists $ENV{DBI_TRACE} and $ENV{DBI_TRACE} != 0 and $ENV{DBI_TRACE} != 42) {
		$maxsize = 1;
	}

	my $runs = 0;

	while (1) {

		last if $runs++ >= $maxsize;

		&$sub();

		unless ($runs % $count) {
			printf "Cycles: %d\tProc size: %uK\n",
				  $runs,
				  (-f "/proc/$$/stat")
				  ? do { local @ARGV="/proc/$$/stat"; (split (/\s/, <>))[22] / 1024 }
				  : -1;
		}


	}

} ## end of leakcheck

__END__
