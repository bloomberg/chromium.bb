# liblouis Braille Translation and Back-Translation Library
#
# Copyright (C) 2017 Bert Frees
#
# This file is part of liblouis.
#
# liblouis is free software: you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 2.1 of the License, or
# (at your option) any later version.
#
# liblouis is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with liblouis. If not, see <http://www.gnu.org/licenses/>.
#

import argparse
from utils import *

def main():
    parser = argparse.ArgumentParser(description="Export chunked words in patgen format.")
    parser.add_argument('-d', '--dictionary', required=True, dest="DICTIONARY",
                        help="dictionary file")
    parser.add_argument('-t', '--table', required=False, dest="TABLE",
                        help="hyphenation patterns to be used for showing the good and bad hyphens found")
    args = parser.parse_args()
    conn, c = open_dictionary(args.DICTIONARY)
    c.execute("SELECT chunked_text FROM dictionary WHERE chunked_text IS NOT NULL")
    if args.TABLE:
        load_table(args.TABLE)
    for chunked_text, in c.fetchall():
        text, hyphen_string = parse_chunks(chunked_text)
        # ignore long words for now
        if len(text) > 48:
            printerrln('WARNING: word is too long to be processed, may be translated incorrectly: %s' % text)
            continue
        if args.TABLE:
            actual_hyphen_string = hyphenate(text)
        else:
            actual_hyphen_string = "".join(["0" * (len(text) - 1)])
        println(my_zip(text,
                       map(lambda e, a: ".0" if e == "x" and a == "1" else
                                        "0" if e == "x" else
                                        "*" if e == "1" and a == "1" else
                                        "-" if e == "1" else
                                        "." if a == "1" else None,
                           hyphen_string, actual_hyphen_string)))
    conn.close()

if __name__ == "__main__": main()