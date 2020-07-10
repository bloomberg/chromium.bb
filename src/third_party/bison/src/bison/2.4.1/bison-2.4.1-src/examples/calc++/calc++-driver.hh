#line 8281 "../../doc/bison.texinfo"
#ifndef CALCXX_DRIVER_HH
# define CALCXX_DRIVER_HH
# include <string>
# include <map>
# include "calc++-parser.hh"
#line 8297 "../../doc/bison.texinfo"
// Tell Flex the lexer's prototype ...
# define YY_DECL                                        \
  yy::calcxx_parser::token_type                         \
  yylex (yy::calcxx_parser::semantic_type* yylval,      \
         yy::calcxx_parser::location_type* yylloc,      \
         calcxx_driver& driver)
// ... and declare it for the parser's sake.
YY_DECL;
#line 8313 "../../doc/bison.texinfo"
// Conducting the whole scanning and parsing of Calc++.
class calcxx_driver
{
public:
  calcxx_driver ();
  virtual ~calcxx_driver ();

  std::map<std::string, int> variables;

  int result;
#line 8331 "../../doc/bison.texinfo"
  // Handling the scanner.
  void scan_begin ();
  void scan_end ();
  bool trace_scanning;
#line 8342 "../../doc/bison.texinfo"
  // Run the parser.  Return 0 on success.
  int parse (const std::string& f);
  std::string file;
  bool trace_parsing;
#line 8356 "../../doc/bison.texinfo"
  // Error handling.
  void error (const yy::location& l, const std::string& m);
  void error (const std::string& m);
};
#endif // ! CALCXX_DRIVER_HH
