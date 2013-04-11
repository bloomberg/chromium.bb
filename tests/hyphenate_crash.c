#include "brl_checks.h"

int main(int argc, char **argv)
{
  return check_hyphenation("cs-g1.ctb,hyph_cs_CZ.dic",
                           "xxxxxxxxxxxxxxx",
                           "000000000000000");
}
