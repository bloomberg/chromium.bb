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

def update_counters(correct, auto_chunked, init=False):
    if not init:
        sys.stderr.write("\033[1A\033[K")
        sys.stderr.write("\033[1A\033[K")
    sys.stderr.write(("%d words correctly translated\n" % correct))
    sys.stderr.write(("%d words automatically chunked\n" % auto_chunked))
    sys.stderr.flush()

def main():
    parser = argparse.ArgumentParser(description="Suggest dictionary rows and liblouis rules")
    parser.add_argument('-d', '--dictionary', required=True, dest="DICTIONARY",
                        help="dictionary file")
    parser.add_argument('-t', '--table', required=True, dest="TABLE",
                        help="translation table including hyphenation patterns")
    parser.add_argument('-n', type=int, default=10, dest="MAX_PROBLEMS",
                        help="maximal number of suggestions")
    parser.add_argument('--auto-chunk', type=bool, default=False, dest="AUTO_CHUNK",
                        help="don't ask for updating chunks")
    parser.add_argument('--print-total-rate', type=bool, default=False, dest="TOTAL_RATE",
                        help="print the total number of correctly translated words (takes longer)")
    args = parser.parse_args()
    println("# -*- tab-width: 30; -*-")
    conn, c = open_dictionary(args.DICTIONARY)
    c.execute("SELECT text,braille,chunked_text FROM dictionary WHERE braille IS NOT NULL OR chunked_text IS NOT NULL ORDER BY frequency DESC")
    load_table(args.TABLE)
    problems = 0
    auto_chunked = 0
    correct = 0
    update_counters(correct, auto_chunked, init=True)
    for text, braille, chunked_text in c.fetchall():
        problem = False
        if braille:
            actual_braille, applied_rules = translate(text)
            if actual_braille != braille:
                problem = True
                if problems < args.MAX_PROBLEMS:
                    comments = []
                    comments.append("wrong braille\t\t" + actual_braille)
                    need_more_hints = True
                    suggest_rows = []
                    suggest_rules = []
                    hyphen_string = future(lambda: parse_chunks(chunked_text or text)[1])
                    actual_hyphen_string = future(lambda: hyphenate(text))
                    non_letters = False
                    if not is_letter(text):
                        non_letters = True
                        comments.append("word has non-letters")
                        if chunked_text:
                            suggest_rows.append({"text": text, "braille": braille})
                            comments.append("word with non-letters should not be chunked")
                    if chunked_text:
                        chunk_errors = compare_chunks(hyphen_string(), actual_hyphen_string(), text)
                        if chunk_errors:
                            comments.append("wrong chunks\t" + chunk_errors)
                            if not non_letters:
                                need_more_hints = False
                    if need_more_hints:
                        suggested_hyphen_string = suggest_chunks(text, braille)
                        if suggested_hyphen_string and suggested_hyphen_string != hyphen_string():
                            if non_letters:
                                subwords = split_into_words(text, suggested_hyphen_string)
                                if subwords:
                                    comments.append("subwords")
                                for subword, suggested_subword_hyphen_string in subwords:
                                    c.execute("SELECT braille,chunked_text FROM dictionary WHERE text=:text", {"text": subword})
                                    fetch = c.fetchone()
                                    if fetch:
                                        subword_braille, chunked_subword = fetch
                                    else:
                                        subword_braille = None
                                        chunked_subword = None
                                        if False: # if args.AUTO_CHUNK
                                            c.execute("INSERT INTO dictionary VALUES (:text,:braille,:chunked_text,0)",
                                                      {"text": subword, "braille": None, "chunked_text": None})
                                    comments.append(">>>\t%s\t%s" % (chunked_subword or subword, subword_braille or ""))
                                    subword_hyphen_string = parse_chunks(chunked_subword or subword)[1]
                                    combined_subword_hyphen_string =\
                                        "".join(map(lambda h1,h2: h1 if h1 == h2 or h2 == 'x' else h2 if h1 == 'x' else '-',
                                                    suggested_subword_hyphen_string, subword_hyphen_string))
                                    if '-' not in combined_subword_hyphen_string:
                                        suggested_subword_hyphen_string = combined_subword_hyphen_string
                                    if suggested_subword_hyphen_string != subword_hyphen_string:
                                        chunk_errors = compare_chunks(suggested_subword_hyphen_string, hyphenate(subword), subword)
                                        if chunk_errors:
                                            comments.append("| wrong chunks\t" + chunk_errors)
                                            suggested_chunked_subword = print_chunks(subword, suggested_subword_hyphen_string)
                                            if not '-' in combined_subword_hyphen_string:
                                                if False: # if args.AUTO_CHUNK
                                                    c.execute("UPDATE dictionary SET chunked_text=:chunked_text WHERE text=:text",
                                                              {"text": subword, "chunked_text": suggested_chunked_subword})
                                                    auto_chunked += 1
                                                    update_counters(correct, auto_chunked)
                                                    need_more_hints = False
                                                    # TODO: set problem to False after all words are auto chunked
                                                else:
                                                    suggest_rows.append({"text": suggested_chunked_subword, "braille": subword_braille})
                            else:
                                chunk_errors = compare_chunks(suggested_hyphen_string, actual_hyphen_string(), text)
                                if chunk_errors:
                                    comments.append("wrong chunks\t" + chunk_errors)
                                    need_more_hints = False
                                    suggested_chunked_text = print_chunks(text, suggested_hyphen_string)
                                    if args.AUTO_CHUNK:
                                        c.execute("UPDATE dictionary SET chunked_text=:chunked_text WHERE text=:text",
                                                  {"text": text, "chunked_text": suggested_chunked_text})
                                        auto_chunked += 1
                                        update_counters(correct, auto_chunked)
                                        problem = False
                                    else:
                                        suggest_rows.append({"text": suggested_chunked_text, "braille": braille})
                    if need_more_hints:
                        comments.append("applied rules")
                        applied_rules = [get_rule(x) for x in applied_rules if x is not None]
                        for rule in applied_rules:
                            comments.append("> " + "\t".join(rule))
                        other_relevant_rules = set(find_relevant_rules(text)) - set(applied_rules)
                        if other_relevant_rules:
                            comments.append("other relevant rules")
                            for rule in other_relevant_rules:
                                comments.append("> " + "\t".join(rule))
                        suggest_rules.append({"opcode": "word", "text": text, "braille": braille})
            if problem and problems < args.MAX_PROBLEMS:
                println("# >>>\t%s\t%s" % (chunked_text or text, braille or ""))
                for comment in comments:
                    println("# | " + comment)
                println("# |__")
                for row in suggest_rows:
                    println("#\t%(text)s\t%(braille)s" % row)
                for rule in suggest_rules:
                    println("# %(opcode)s\t%(text)s\t%(braille)s" % rule)
                    println()
                problems += 1
                if not args.TOTAL_RATE and problems >= args.MAX_PROBLEMS:
                    break
            else:
                correct += 1
                update_counters(correct, auto_chunked)
    if args.TOTAL_RATE:
        println("### %d out of %d (%.1f %%) words translated correctly" % (correct, correct + problems, math.floor(1000 * correct / (correct + problems)) / 10))
    elif correct > 0:
        println("### %d words translated correctly" % correct)
    if auto_chunked > 0:
        println("### %d words automatically chunked" % auto_chunked)
        conn.commit()
    conn.close()

if __name__ == "__main__": main()