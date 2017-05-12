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
        if args.TABLE:
            actual_hyphen_string = hyphenate(text)
        else:
            actual_hyphen_string = ["0" * (len(text) - 1)]
        println(my_zip(text,
                       map(lambda e, a: ".0" if e == "x" and a == "1" else
                                        "0" if e == "x" else
                                        "*" if e == "1" and a == "1" else
                                        "-" if e == "1" else
                                        "." if a == "1" else None,
                           hyphen_string, actual_hyphen_string)))
    conn.close()

if __name__ == "__main__": main()