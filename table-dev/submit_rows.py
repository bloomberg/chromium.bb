import argparse
import csv
import fileinput
from utils import *

class Reader:
    def __init__(self, files, encoding):
        self.reader = csv.reader(fileinput.FileInput(files, openhook=fileinput.hook_encoded(encoding)),
                                 delimiter='\t', quoting=csv.QUOTE_NONE)
    def __iter__(self):
        return self
    def __next__(self):
        row = next(self.reader)
        if not row or row[0].startswith("#"):
            return next(self)
        if not len(row) == 3:
            printerrln('expected 3 columns, got %s: %s' % (len(row),row))
            exit(1)
        if not row[0] == "":
            printerrln("expected first column to be empty, got '%s'" % (row[0],))
            exit(1)
        maybe_chunked_text, braille = row[1:3]
        maybe_chunked_text = to_lowercase(maybe_chunked_text)
        text, chunked_text = read_text(maybe_chunked_text)
        braille = braille if braille != "" else None
        if braille != None:
            if '0' in to_dot_pattern(braille).split('-'):
                printerrln('invalid braille: %s' % (braille,))
                exit(1)
        exit_if_not(not chunked_text or validate_chunks(chunked_text))
        return {'text': text,
                'braille': braille,
                'chunked_text': chunked_text}

def main():
    parser = argparse.ArgumentParser(description="Submit entries to dictionary.")
    parser.add_argument('FILE', nargs='*', default=['-'], help="TSV file with entries")
    parser.add_argument('-d', '--dictionary', required=True, dest="DICTIONARY",
                        help="dictionary file")
    parser.add_argument('-t', '--table', required=True, dest="TABLE",
                        help="translation table to be used for converting words to lowercase")
    args = parser.parse_args()
    load_table(args.TABLE)
    conn, c = open_dictionary(args.DICTIONARY)
    reader = Reader(args.FILE, "utf-8")
    c.execute("CREATE TABLE IF NOT EXISTS dictionary (text TEXT PRIMARY KEY, braille TEXT, chunked_text TEXT, frequency INTEGER)")
    for row in reader:
        c.execute("INSERT OR IGNORE INTO dictionary VALUES (:text,:braille,:chunked_text,0)", row)
        if c.lastrowid == 0:
            c.execute("UPDATE dictionary SET braille=:braille,chunked_text=:chunked_text WHERE text=:text", row)
    conn.commit()
    conn.close()

if __name__ == "__main__": main()