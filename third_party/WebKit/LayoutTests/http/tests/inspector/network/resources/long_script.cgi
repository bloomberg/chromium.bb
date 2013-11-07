#!/usr/bin/perl -wT
use strict;

my @out;
push(@out, "Content-type: text/plain\n");
push(@out, "Cache-Control: no-cache, no-store\n");
push(@out, "\n");

my @fib = (8, 18, 2, 28, 59, 41, 34, 27, 16, 13);
my $i = 0;
my (@buffer, @chars, $j, $k, $chunk);

for ($j = 0; $j < 64; $j++) {
  push(@chars, chr($j + 32));
}

for ($j = 0; $j < 33; $j++) {
  push(@buffer, "// ");
  for ($k = 0; $k < 1020; $k++) {
    push(@buffer, @chars[@fib[$i % 10]]);
    @fib[$i % 10] = (@fib[$i % 10] + @fib[($i + 3) % 10]) % 64;
    $i++;
  }
  push(@buffer, "\n");
}

$chunk = join("", @buffer);
for ($j = 0; $j < 250; $j++) {
  push(@out, $chunk);
}
print join("", @out);
