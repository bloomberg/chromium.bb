package SQL::Translator::Producer::DBIx::Class::File;

=head1 NAME

SQL::Translator::Producer::DBIx::Class::File - DBIx::Class file producer

=head1 SYNOPSIS

  use SQL::Translator;

  my $t = SQL::Translator->new( parser => '...',
                                producer => 'DBIx::Class::File' );
  print $translator->translate( $file );

=head1 DESCRIPTION

Creates a DBIx::Class::Schema for use with DBIx::Class

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.

=cut

use strict;
our ($VERSION, $DEBUG, $WARN);
$VERSION = '0.1';
$DEBUG   = 0 unless defined $DEBUG;

use SQL::Translator::Schema::Constants;
use SQL::Translator::Utils qw(header_comment);
use Data::Dumper ();

## Skip all column type translation, as we want to use whatever the parser got.

## Translate parsers -> PK::Auto::Foo, however

my %parser2PK = (
                 MySQL       => 'PK::Auto::MySQL',
                 PostgreSQL  => 'PK::Auto::Pg',
                 DB2         => 'PK::Auto::DB2',
                 Oracle      => 'PK::Auto::Oracle',
                 );

sub produce
{
    my ($translator) = @_;
    $DEBUG             = $translator->debug;
    $WARN              = $translator->show_warnings;
    my $no_comments    = $translator->no_comments;
    my $add_drop_table = $translator->add_drop_table;
    my $schema         = $translator->schema;
    my $output         = '';

    # Steal the XML producers "prefix" arg for our namespace?
    my $dbixschema     = $translator->producer_args()->{prefix} ||
        $schema->name || 'My::Schema';
    my $pkclass = $parser2PK{$translator->parser_type} || '';

    my %tt_vars = ();
    $tt_vars{dbixschema} = $dbixschema;
    $tt_vars{pkclass} = $pkclass;

    my $schemaoutput .= << "DATA";

package ${dbixschema};
use base 'DBIx::Class::Schema';
use strict;
use warnings;
DATA

    my %tableoutput = ();
    my %tableextras = ();
    foreach my $table ($schema->get_tables)
    {
        my $tname = $table->name;
        my $output .= qq{

package ${dbixschema}::${tname};
use base 'DBIx::Class';
use strict;
use warnings;

__PACKAGE__->load_components(qw/${pkclass} Core/);
__PACKAGE__->table('${tname}');

};

        my @fields = map
        { { $_->name  => {
            name              => $_->name,
            is_auto_increment => $_->is_auto_increment,
            is_foreign_key    => $_->is_foreign_key,
            is_nullable       => $_->is_nullable,
            default_value     => $_->default_value,
            data_type         => $_->data_type,
            size              => $_->size,
        } }
         } ($table->get_fields);

        $output .= "\n__PACKAGE__->add_columns(";
        foreach my $f (@fields)
        {
            local $Data::Dumper::Terse = 1;
            $output .= "\n    '" . (keys %$f)[0] . "' => " ;
            my $colinfo =
                Data::Dumper->Dump([values %$f],
                                   [''] # keys   %$f]
                                   );
            chomp($colinfo);
            $output .= $colinfo . ",";
        }
        $output .= "\n);\n";

        my $pk = $table->primary_key;
        if($pk)
        {
            my @pk = map { $_->name } ($pk->fields);
            $output .= "__PACKAGE__->set_primary_key(";
            $output .= "'" . join("', '", @pk) . "');\n";
        }

        foreach my $cont ($table->get_constraints)
        {
#            print Data::Dumper::Dumper($cont->type);
            if($cont->type =~ /foreign key/i)
            {
#                 $output .= "\n__PACKAGE__->belongs_to('" .
#                     $cont->fields->[0]->name . "', '" .
#                     "${dbixschema}::" . $cont->reference_table . "');\n";

                $tableextras{$table->name} .= "\n__PACKAGE__->belongs_to('" .
                    $cont->fields->[0]->name . "', '" .
                    "${dbixschema}::" . $cont->reference_table . "');\n";

                my $other = "\n__PACKAGE__->has_many('" .
                    "get_" . $table->name. "', '" .
                    "${dbixschema}::" . $table->name. "', '" .
                    $cont->fields->[0]->name . "');";
                $tableextras{$cont->reference_table} .= $other;
            }
        }

        $tableoutput{$table->name} .= $output;
    }

    foreach my $to (keys %tableoutput)
    {
        $output .= $tableoutput{$to};
        $schemaoutput .= "\n__PACKAGE__->register_class('${to}', '${dbixschema}::${to}');\n";
    }

    foreach my $te (keys %tableextras)
    {
        $output .= "\npackage ${dbixschema}::$te;\n";
        $output .= $tableextras{$te} . "\n";
#        $tableoutput{$te} .= $tableextras{$te} . "\n";
    }

#    print "$output\n";
    return "${output}\n\n${schemaoutput}\n1;\n";
}
