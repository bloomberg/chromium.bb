{print $0}
b=$0
/f[1-9]?f[1-9]?i/ {gsub("f[1-9]?f[1-9]?i", "ﬃ",b) }
/f[1-9]?f[1-9]?l/ {gsub("f[1-9]?f[1-9]?l", "ﬄ",b) }

/f[1-9]?f/ {gsub("f[1-9]?f", "ﬀ",b); r=1}
/f[1-9]?i/ {gsub("f[1-9]?i", "ﬁ",b); r=1}
/f[1-9]?l/ {gsub("f[1-9]?l", "ﬂ",b); r=1}

b!=$0 { 
    # check alternative hyph
    if (num ~ /\//) {
	next;
    }
    print b
}

c=b

/^[1-9]?f/ { sub("^[1-9]?f", "ﬀ", c); }
/^[1-9]?i/ { sub("^[1-9]?i", "ﬁ", c); }
/^[1-9]?l/ { sub("^[1-9]?l", "ﬂ", c); }

c!=b { print c }

/f[1-9]?$/ {
	print gensub("f[1-9]?$", "ﬀ", "g", b);
	if (c!=b) print gensub("f[1-9]?$", "ﬀ", "g", c);

	print gensub("f[1-9]?$", "ﬁ", "g", b);
	if (c!=b) print gensub("f[1-9]?$", "ﬁ", "g", c);

	print gensub("f[1-9]?$", "ﬂ", "g", b);
	if (c!=b) print gensub("f[1-9]?$", "ﬂ", "g", c);
}


#s/ffi/ﬃ/g
#s/ffl/ﬄ/g
#s/ff/ﬀ/g
#s/fi/ﬁ/g
#s/fl/ﬂ/g
