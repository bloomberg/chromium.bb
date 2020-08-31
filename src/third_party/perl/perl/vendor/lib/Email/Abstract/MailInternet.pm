use strict;
package Email::Abstract::MailInternet;
# ABSTRACT: Email::Abstract wrapper for Mail::Internet
$Email::Abstract::MailInternet::VERSION = '3.008';
use Email::Abstract::Plugin;
BEGIN { @Email::Abstract::MailInternet::ISA = 'Email::Abstract::Plugin' };

sub target { "Mail::Internet" }

# We need 1.77 because otherwise headers unfold badly.
my $is_avail;
sub is_available {
  return $is_avail if defined $is_avail;
  require Mail::Internet;
  eval { Mail::Internet->VERSION(1.77) };
  return $is_avail = $@ ? 0 : 1;
}

sub construct {
    require Mail::Internet;
    my ($class, $rfc822) = @_;
    Mail::Internet->new([ map { "$_\x0d\x0a" } split /\x0d\x0a/, $rfc822]);
}

sub get_header {
    my ($class, $obj, $header) = @_;
    my @values = $obj->head->get($header);
    return unless @values;

    # No reason to s/// lots of values if we're just going to return one.
    $#values = 0 if not wantarray;

    chomp @values;
    s/(?:\x0d\x0a|\x0a\x0d|\x0a|\x0d)\s+/ /g for @values;

    return wantarray ? @values : $values[0];
}

sub get_body {
    my ($class, $obj) = @_;
    join "", @{$obj->body()};
}

sub set_header {
    my ($class, $obj, $header, @data) = @_;
    my $count = 0;
    $obj->head->replace($header, shift @data, ++$count) while @data;
}

sub set_body {
    my ($class, $obj, $body) = @_;
    $obj->body( map { "$_\n" } split /\n/, $body );
}

sub as_string { my ($class, $obj) = @_; $obj->as_string(); }

1;

#pod =head1 DESCRIPTION
#pod
#pod This module wraps the Mail::Internet mail handling library with an
#pod abstract interface, to be used with L<Email::Abstract>
#pod
#pod =head1 SEE ALSO
#pod
#pod L<Email::Abstract>, L<Mail::Internet>.
#pod
#pod =cut

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::Abstract::MailInternet - Email::Abstract wrapper for Mail::Internet

=head1 VERSION

version 3.008

=head1 DESCRIPTION

This module wraps the Mail::Internet mail handling library with an
abstract interface, to be used with L<Email::Abstract>

=head1 SEE ALSO

L<Email::Abstract>, L<Mail::Internet>.

=head1 AUTHORS

=over 4

=item *

Ricardo SIGNES <rjbs@cpan.org>

=item *

Simon Cozens <simon@cpan.org>

=item *

Casey West <casey@geeknest.com>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2004 by Simon Cozens.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
