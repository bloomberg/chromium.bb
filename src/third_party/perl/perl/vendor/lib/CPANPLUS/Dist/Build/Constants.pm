package CPANPLUS::Dist::Build::Constants;
$CPANPLUS::Dist::Build::Constants::VERSION = '0.90';
#ABSTRACT: Constants for CPANPLUS::Dist::Build

use if $] > 5.017, 'deprecate';

use strict;
use warnings;
use File::Spec;

BEGIN {

    require Exporter;
    use vars    qw[@ISA @EXPORT];

    @ISA        = qw[Exporter];
    @EXPORT     = qw[ BUILD_DIR BUILD CPDB_PERL_WRAPPER];
}


use constant BUILD_DIR      => sub { return @_
                                        ? File::Spec->catdir($_[0], '_build')
                                        : '_build';
                            };
use constant BUILD          => sub { my $file = @_
                                        ? File::Spec->catfile($_[0], 'Build')
                                        : 'Build';

                                     ### on VMS, '.com' is appended when
                                     ### creating the Build file
                                     $file .= '.com' if $^O eq 'VMS';

                                     return $file;
                            };


use constant CPDB_PERL_WRAPPER   => 'use strict; BEGIN { my $old = select STDERR; $|++; select $old; $|++; $0 = shift(@ARGV); my $rv = do($0); die $@ if $@; }';

1;


# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:

__END__

=pod

=encoding UTF-8

=head1 NAME

CPANPLUS::Dist::Build::Constants - Constants for CPANPLUS::Dist::Build

=head1 VERSION

version 0.90

=head1 SYNOPSIS

  use CPANPLUS::Dist::Build::Constants;

=head1 DESCRIPTION

CPANPLUS::Dist::Build::Constants provides some constants required by L<CPANPLUS::Dist::Build>.

=head1 PROMINENCE

Originally by Jos Boumans E<lt>kane@cpan.orgE<gt>.  Brought to working
condition and currently maintained by Ken Williams E<lt>kwilliams@cpan.orgE<gt>.

=head1 AUTHOR

Jos Boumans <kane[at]cpan.org>, Ken Williams <kwilliams@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2018 by Jos Boumans, Ken Williams, Chris Williams and David Golden.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
