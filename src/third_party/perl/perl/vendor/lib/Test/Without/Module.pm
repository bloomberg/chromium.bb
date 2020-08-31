package Test::Without::Module;
use strict;
use Carp qw( croak );

use vars qw( $VERSION );
$VERSION = '0.20';

use vars qw(%forbidden);

sub get_forbidden_list {
  \%forbidden
};

sub import {
  my ($self,@forbidden_modules) = @_;

  my $forbidden = get_forbidden_list;
  
  for (@forbidden_modules) {
      my $file = module2file($_);
      $forbidden->{$file} = delete $INC{$file};
  };

  # Scrub %INC, so that loaded modules disappear
  for my $module (@forbidden_modules) {
    scrub( $module );
  };

  @INC = (\&fake_module, grep { !ref || $_ != \&fake_module } @INC);
};

sub fake_module {
    my ($self,$module_file,$member_only) = @_;
    # Don't touch $@, or .al files will not load anymore????

    if (exists get_forbidden_list->{$module_file}) {
      my $module_name = file2module($module_file);
      croak "Can't locate $module_file in \@INC (you may need to install the $module_name module) (\@INC contains: @INC)";
    };
};

sub unimport {
  my ($self,@list) = @_;
  my $module;
  my $forbidden = get_forbidden_list;

  for $module (@list) {
    my $file = module2file($module);
    if (exists $forbidden->{$file}) {
      my $path = delete $forbidden->{$file};
      if (defined $path) {
        $INC{ $file } = $path;
      }
    } else {
      croak "Can't allow non-forbidden module $module";
    };
  };
};

sub file2module {
  my ($mod) = @_;
  $mod =~ s!/!::!g;
  $mod =~ s!\.pm$!!;
  $mod;
};

sub module2file {
  my ($mod) = @_;
  $mod =~ s!::|'!/!g;
  $mod .= ".pm";
  $mod;
};

sub scrub {
  my ($module) = @_;
  delete $INC{module2file($module)};
};

1;

=head1 NAME

Test::Without::Module - Test fallback behaviour in absence of modules

=head1 SYNOPSIS

=for example begin

  use Test::Without::Module qw( My::Module );

  # Now, loading of My::Module fails :
  eval { require My::Module; };
  warn $@ if $@;

  # Now it works again
  eval q{ no Test::Without::Module qw( My::Module ) };
  eval { require My::Module; };
  print "Found My::Module" unless $@;

=for example end

=head1 DESCRIPTION

This module allows you to deliberately hide modules from a program
even though they are installed. This is mostly useful for testing modules
that have a fallback when a certain dependency module is not installed.

=head2 EXPORT

None. All magic is done via C<use Test::Without::Module LIST> and
C<no Test::Without::Module LIST>.

=head2 Test::Without::Module::get_forbidden_list

This function returns a reference to a copy of the current hash of forbidden
modules or an empty hash if none are currently forbidden. This is convenient
if you are testing and/or debugging this module.

=cut

=head1 ONE LINER

A neat trick for using this module from the command line
was mentioned to me by NUFFIN and by Jerrad Pierce:

  perl -MTest::Without::Module=Some::Module -w -Iblib/lib t/SomeModule.t

That way, you can easily see how your module or test file behaves
when a certain module is unavailable.

=head1 BUGS

=over 4

=item *

There is no lexical scoping

=back

=head1 CREDITS

Much improvement must be thanked to Aristotle from PerlMonks, he pointed me
to a much less convoluted way to fake a module at
L<https://perlmonks.org?node=192635>.

I also discussed with him an even more elegant way of overriding
CORE::GLOBAL::require, but the parsing of the overridden subroutine
didn't work out the way I wanted it - CORE::require didn't recognize
barewords as such anymore.

NUFFIN and Jerrad Pierce pointed out the convenient
use from the command line to interactively watch the
behaviour of the test suite and module in absence
of a module.

=head1 AUTHOR

Copyright (c) 2003-2014 Max Maischein, E<lt>corion@cpan.orgE<gt>

=head1 LICENSE

This module is released under the same terms as Perl itself.

=head1 REPOSITORY

The public repository of this module is
L<https://github.com/Corion/test-without-module>.

=head1 SUPPORT

The public support forum of this module is
L<https://perlmonks.org/>.

=head1 BUG TRACKER

Please report bugs in this module via the RT CPAN bug queue at
L<https://rt.cpan.org/Public/Dist/Display.html?Name=Test-Without-Module>
or via mail to L<test-without-module-Bugs@rt.cpan.org>.

=head1 SEE ALSO

L<Devel::Hide>, L<Acme::Intraweb>, L<PAR>, L<perlfunc>

=cut

__END__
