use 5.006;    # pragmas
use strict;
use warnings;

package Test::File::ShareDir::Module;

our $VERSION = '1.001002';

# ABSTRACT: Simplified module oriented ShareDir tester

our $AUTHORITY = 'cpan:KENTNL'; # AUTHORITY

use File::ShareDir 1.00 qw();
use Test::File::ShareDir::Utils qw( extract_dashes );












sub import {
  my ( undef, $arg ) = @_;

  if ( not ref $arg or 'HASH' ne ref $arg ) {
    require Carp;
    return Carp::croak q[Must pass a hashref];
  }

  my %input_config = %{$arg};

  require Test::File::ShareDir::Object::Module;

  my $module_object = Test::File::ShareDir::Object::Module->new(extract_dashes('modules', \%input_config ));
  $module_object->install_all_modules();
  $module_object->register();
  return 1;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test::File::ShareDir::Module - Simplified module oriented ShareDir tester

=head1 VERSION

version 1.001002

=head1 SYNOPSIS

    use Test::File::ShareDir::Module {
        '-root' => "some/root/path",
        'Module::Foo' => "share/ModuleFoo",
    };

C<-root> is optional, and defaults to C<cwd>

B<NOTE:> There's a bug prior to 5.18 with C<< use Foo { -key => } >>, so for backwards compatibility, make sure you either quote
the key: C<< use Foo { '-key' => } >>, or make it the non-first key.

=begin MetaPOD::JSON v1.1.0

{
    "namespace":"Test::File::ShareDir::Module",
    "interface":"exporter"
}


=end MetaPOD::JSON

=head1 AUTHOR

Kent Fredric <kentnl@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2017 by Kent Fredric <kentnl@cpan.org>.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
