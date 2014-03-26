// Copyright (C) 2013 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// The data in this file will be automatically generated. For now, the address
// data comes from:
//
// https://code.google.com/p/libaddressinput/source/browse/trunk/java/src/com/android/i18n/addressinput/RegionDataConstants.java?r=137
//
// The language-to-separator mapping and primary input languages are loosely
// based on:
//
// http://unicode.org/cldr/trac/browser/tags/release-24/common/supplemental/supplementalData.xml

#include "region_data_constants.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace i18n {
namespace addressinput {

namespace {

std::map<std::string, std::string> InitRegionData() {
  std::map<std::string, std::string> region_data;
  region_data.insert(std::make_pair("AD", "{"
      "\"name\":\"ANDORRA\","
      "\"lang\":\"ca\","
      "\"languages\":\"ca\","
      "\"input_languages\":\"ca\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %S\","
      "\"require\":\"AS\","
      "\"state_name_type\":\"parish\""
      "}"));
  region_data.insert(std::make_pair("AE", "{"
      "\"name\":\"UNITED ARAB EMIRATES\","
      "\"input_languages\":\"ar\","
      "\"fmt\":\"%N%n%O%n%A%n%C\","
      "\"require\":\"AC\""
      "}"));
  region_data.insert(std::make_pair("AF", "{"
      "\"name\":\"AFGHANISTAN\","
      "\"input_languages\":\"fa~ps\""
      "}"));
  region_data.insert(std::make_pair("AG", "{"
      "\"name\":\"ANTIGUA AND BARBUDA\","
      "\"input_languages\":\"en\","
      "\"require\":\"A\""
      "}"));
  region_data.insert(std::make_pair("AI", "{"
      "\"name\":\"ANGUILLA\","
      "\"input_languages\":\"en\""
      "}"));
  region_data.insert(std::make_pair("AL", "{"
      "\"name\":\"ALBANIA\","
      "\"input_languages\":\"sq\""
      "}"));
  region_data.insert(std::make_pair("AM", "{"
      "\"name\":\"ARMENIA\","
      "\"lang\":\"hy\","
      "\"languages\":\"hy\","
      "\"input_languages\":\"hy\","
      "\"fmt\":\"%N%n%O%n%A%n%Z%n%C%n%S\","
      "\"lfmt\":\"%N%n%O%n%A%n%Z%n%C%n%S\""
      "}"));
  region_data.insert(std::make_pair("AN", "{"
      "\"name\":\"NETHERLANDS ANTILLES\""
      "}"));
  region_data.insert(std::make_pair("AO", "{"
      "\"name\":\"ANGOLA\","
      "\"input_languages\":\"pt\""
      "}"));
  region_data.insert(std::make_pair("AQ", "{"
      "\"name\":\"ANTARCTICA\""
      "}"));
  region_data.insert(std::make_pair("AR", "{"
      "\"name\":\"ARGENTINA\","
      "\"lang\":\"es\","
      "\"languages\":\"es\","
      "\"input_languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C%n%S\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("AS", "{"
      "\"name\":\"AMERICAN SAMOA\","
      "\"input_languages\":\"en~sm\","
      "\"fmt\":\"%N%n%O%n%A%n%C %S %Z\","
      "\"require\":\"ACSZ\","
      "\"zip_name_type\":\"zip\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("AT", "{"
      "\"name\":\"AUSTRIA\","
      "\"input_languages\":\"de\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("AU", "{"
      "\"name\":\"AUSTRALIA\","
      "\"lang\":\"en\","
      "\"languages\":\"en\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%O%n%N%n%A%n%C %S %Z\","
      "\"require\":\"ACSZ\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("AW", "{"
      "\"name\":\"ARUBA\","
      "\"input_languages\":\"nl~pap\""
      "}"));
  region_data.insert(std::make_pair("AX", "{"
      "\"name\":\"FINLAND\","
      "\"input_languages\":\"sv\","
      "\"fmt\":\"%O%n%N%n%A%nAX-%Z %C%n\xC3\x85LAND\","  // \xC3\x85 is Å.
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("AZ", "{"
      "\"name\":\"AZERBAIJAN\","
      "\"input_languages\":\"az\","
      "\"fmt\":\"%N%n%O%n%A%nAZ %Z %C\""
      "}"));
  region_data.insert(std::make_pair("BA", "{"
      "\"name\":\"BOSNIA AND HERZEGOVINA\","
      "\"input_languages\":\"bs~hr~sr\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("BB", "{"
      "\"name\":\"BARBADOS\","
      "\"input_languages\":\"en\","
      "\"state_name_type\":\"parish\""
      "}"));
  region_data.insert(std::make_pair("BD", "{"
      "\"name\":\"BANGLADESH\","
      "\"input_languages\":\"bn\","
      "\"fmt\":\"%N%n%O%n%A%n%C - %Z\""
      "}"));
  region_data.insert(std::make_pair("BE", "{"
      "\"name\":\"BELGIUM\","
      "\"input_languages\":\"de~fr~nl\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("BF", "{"
      "\"name\":\"BURKINA FASO\","
      "\"input_languages\":\"fr\","
      "\"fmt\":\"%N%n%O%n%A%n%C %X\""
      "}"));
  region_data.insert(std::make_pair("BG", "{"
      "\"name\":\"BULGARIA (REP.)\","
      "\"input_languages\":\"bg\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("BH", "{"
      "\"name\":\"BAHRAIN\","
      "\"input_languages\":\"ar\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("BI", "{"
      "\"name\":\"BURUNDI\","
      "\"input_languages\":\"fr~rn\""
      "}"));
  region_data.insert(std::make_pair("BJ", "{"
      "\"name\":\"BENIN\","
      "\"input_languages\":\"fr\""
      "}"));
  region_data.insert(std::make_pair("BL", "{"
      "\"name\":\"SAINT BARTHELEMY\","
      "\"input_languages\":\"fr\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("BM", "{"
      "\"name\":\"BERMUDA\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("BN", "{"
      "\"name\":\"BRUNEI DARUSSALAM\","
      "\"input_languages\":\"ms\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("BO", "{"
      "\"name\":\"BOLIVIA\","
      "\"input_languages\":\"ay~es~qu\""
      "}"));
  region_data.insert(std::make_pair("BR", "{"
      "\"name\":\"BRAZIL\","
      "\"lang\":\"pt\","
      "\"languages\":\"pt\","
      "\"input_languages\":\"pt\","
      "\"fmt\":\"%O%n%N%n%A%n%C-%S%n%Z\","
      "\"require\":\"ASCZ\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("BS", "{"
      "\"name\":\"BAHAMAS\","
      "\"lang\":\"en\","
      "\"languages\":\"en\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%C, %S\","
      "\"state_name_type\":\"island\""
      "}"));
  region_data.insert(std::make_pair("BT", "{"
      "\"name\":\"BHUTAN\","
      "\"input_languages\":\"dz\""
      "}"));
  region_data.insert(std::make_pair("BV", "{"
      "\"name\":\"BOUVET ISLAND\""
      "}"));
  region_data.insert(std::make_pair("BW", "{"
      "\"name\":\"BOTSWANA\","
      "\"input_languages\":\"en~tn\""
      "}"));
  region_data.insert(std::make_pair("BY", "{"
      "\"name\":\"BELARUS\","
      "\"input_languages\":\"be~ru\","
      "\"fmt\":\"%S%n%Z %C %X%n%A%n%O%n%N\""
      "}"));
  region_data.insert(std::make_pair("BZ", "{"
      "\"name\":\"BELIZE\","
      "\"input_languages\":\"en\""
      "}"));
  region_data.insert(std::make_pair("CA", "{"
      "\"name\":\"CANADA\","
      "\"lang\":\"en\","
      "\"languages\":\"en~fr\","
      "\"input_languages\":\"en~fr\","
      "\"fmt\":\"%N%n%O%n%A%n%C %S %Z\","
      "\"require\":\"ACSZ\""
      "}"));
  region_data.insert(std::make_pair("CC", "{"
      "\"name\":\"COCOS (KEELING) ISLANDS\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%O%n%N%n%A%n%C %S %Z\""
      "}"));
  region_data.insert(std::make_pair("CD", "{"
      "\"name\":\"CONGO (DEM. REP.)\","
      "\"input_languages\":\"fr\","
      "\"fmt\":\"%N%n%O%n%A%n%C %X\""
      "}"));
  region_data.insert(std::make_pair("CF", "{"
      "\"name\":\"CENTRAL AFRICAN REPUBLIC\","
      "\"input_languages\":\"fr~sg\""
      "}"));
  region_data.insert(std::make_pair("CG", "{"
      "\"name\":\"CONGO (REP.)\","
      "\"input_languages\":\"fr~ln\""
      "}"));
  region_data.insert(std::make_pair("CH", "{"
      "\"name\":\"SWITZERLAND\","
      "\"lang\":\"de\","
      "\"languages\":\"de~fr~it\","
      "\"input_languages\":\"de~fr~gsw~it\","
      "\"fmt\":\"%O%n%N%n%A%nCH-%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("CI", "{"
      "\"name\":\"COTE D'IVOIRE\","
      "\"input_languages\":\"fr\","
      "\"fmt\":\"%N%n%O%n%X %A %C %X\""
      "}"));
  region_data.insert(std::make_pair("CK", "{"
      "\"name\":\"COOK ISLANDS\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("CL", "{"
      "\"name\":\"CHILE\","
      "\"lang\":\"es\","
      "\"languages\":\"es\","
      "\"input_languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C%n%S\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("CM", "{"
      "\"name\":\"CAMEROON\","
      "\"input_languages\":\"en~fr\""
      "}"));
  region_data.insert(std::make_pair("CN", "{"
      "\"name\":\"P.R. CHINA\","
      "\"lang\":\"zh-hans\","
      "\"languages\":\"zh-hans\","
      "\"input_languages\":\"zh\","
      "\"fmt\":\"%Z%n%S%C%D%n%A%n%O%n%N\","
      "\"lfmt\":\"%N%n%O%n%A, %D%n%C%n%S, %Z\","
      "\"require\":\"ACSZ\""
      "}"));
  region_data.insert(std::make_pair("CO", "{"
      "\"name\":\"COLOMBIA\","
      "\"input_languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%C, %S\""
      "}"));
  region_data.insert(std::make_pair("CR", "{"
      "\"name\":\"COSTA RICA\","
      "\"input_languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("CS", "{"
      "\"name\":\"SERBIA AND MONTENEGRO\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("CV", "{"
      "\"name\":\"CAPE VERDE\","
      "\"lang\":\"pt\","
      "\"languages\":\"pt\","
      "\"input_languages\":\"pt\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C%n%S\","
      "\"state_name_type\":\"island\""
      "}"));
  region_data.insert(std::make_pair("CX", "{"
      "\"name\":\"CHRISTMAS ISLAND\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%O%n%N%n%A%n%C %S %Z\""
      "}"));
  region_data.insert(std::make_pair("CY", "{"
      "\"name\":\"CYPRUS\","
      "\"input_languages\":\"el~tr\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("CZ", "{"
      "\"name\":\"CZECH REP.\","
      "\"input_languages\":\"cs\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("DE", "{"
      "\"name\":\"GERMANY\","
      "\"input_languages\":\"de\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("DJ", "{"
      "\"name\":\"DJIBOUTI\","
      "\"input_languages\":\"ar~fr\""
      "}"));
  region_data.insert(std::make_pair("DK", "{"
      "\"name\":\"DENMARK\","
      "\"input_languages\":\"da\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("DM", "{"
      "\"name\":\"DOMINICA\","
      "\"input_languages\":\"en\""
      "}"));
  region_data.insert(std::make_pair("DO", "{"
      "\"name\":\"DOMINICAN REP.\","
      "\"input_languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("DZ", "{"
      "\"name\":\"ALGERIA\","
      "\"input_languages\":\"ar~fr\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("EC", "{"
      "\"name\":\"ECUADOR\","
      "\"input_languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%Z%n%C\""
      "}"));
  region_data.insert(std::make_pair("EE", "{"
      "\"name\":\"ESTONIA\","
      "\"input_languages\":\"et\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("EG", "{"
      "\"name\":\"EGYPT\","
      "\"input_languages\":\"ar\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%S%n%Z\""
      "}"));
  region_data.insert(std::make_pair("EH", "{"
      "\"name\":\"WESTERN SAHARA\","
      "\"input_languages\":\"ar\""
      "}"));
  region_data.insert(std::make_pair("ER", "{"
      "\"name\":\"ERITREA\","
      "\"input_languages\":\"ar~en~ti\""
      "}"));
  region_data.insert(std::make_pair("ES", "{"
      "\"name\":\"SPAIN\","
      "\"lang\":\"es\","
      "\"languages\":\"es\","
      "\"input_languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C %S\","
      "\"require\":\"ACSZ\""
      "}"));
  region_data.insert(std::make_pair("ET", "{"
      "\"name\":\"ETHIOPIA\","
      "\"input_languages\":\"am\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("FI", "{"
      "\"name\":\"FINLAND\","
      "\"input_languages\":\"fi~sv\","
      "\"fmt\":\"%O%n%N%n%A%nFI-%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("FJ", "{"
      "\"name\":\"FIJI\","
      "\"input_languages\":\"en~fj\""
      "}"));
  region_data.insert(std::make_pair("FK", "{"
      "\"name\":\"FALKLAND ISLANDS (MALVINAS)\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%X%n%C%n%Z\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("FM", "{"
      "\"name\":\"MICRONESIA (Federated State of)\","
      "\"input_languages\":\"chk~en~kos~pon~uli~yap\","
      "\"fmt\":\"%N%n%O%n%A%n%C %S %Z\","
      "\"require\":\"ACSZ\","
      "\"zip_name_type\":\"zip\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("FO", "{"
      "\"name\":\"FAROE ISLANDS\","
      "\"input_languages\":\"fo\","
      "\"fmt\":\"%N%n%O%n%A%nFO%Z %C\""
      "}"));
  region_data.insert(std::make_pair("FR", "{"
      "\"name\":\"FRANCE\","
      "\"input_languages\":\"fr\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("GA", "{"
      "\"name\":\"GABON\","
      "\"input_languages\":\"fr\""
      "}"));
  region_data.insert(std::make_pair("GB", "{"
      "\"name\":\"UNITED KINGDOM\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%S%n%Z\","
      "\"require\":\"ACZ\","
      "\"state_name_type\":\"county\""
      "}"));
  region_data.insert(std::make_pair("GD", "{"
      "\"name\":\"GRENADA (WEST INDIES)\","
      "\"input_languages\":\"en\""
      "}"));
  region_data.insert(std::make_pair("GE", "{"
      "\"name\":\"GEORGIA\","
      "\"input_languages\":\"ka\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("GF", "{"
      "\"name\":\"FRENCH GUIANA\","
      "\"input_languages\":\"fr\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("GG", "{"
      "\"name\":\"CHANNEL ISLANDS\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%X%n%C%nGUERNSEY%n%Z\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("GH", "{"
      "\"name\":\"GHANA\","
      "\"input_languages\":\"en\""
      "}"));
  region_data.insert(std::make_pair("GI", "{"
      "\"name\":\"GIBRALTAR\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A\","
      "\"require\":\"A\""
      "}"));
  region_data.insert(std::make_pair("GL", "{"
      "\"name\":\"GREENLAND\","
      "\"input_languages\":\"da~kl\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("GM", "{"
      "\"name\":\"GAMBIA\","
      "\"input_languages\":\"en\""
      "}"));
  region_data.insert(std::make_pair("GN", "{"
      "\"name\":\"GUINEA\","
      "\"input_languages\":\"fr\","
      "\"fmt\":\"%N%n%O%n%Z %A %C\""
      "}"));
  region_data.insert(std::make_pair("GP", "{"
      "\"name\":\"GUADELOUPE\","
      "\"input_languages\":\"fr\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("GQ", "{"
      "\"name\":\"EQUATORIAL GUINEA\","
      "\"input_languages\":\"es~fr\""
      "}"));
  region_data.insert(std::make_pair("GR", "{"
      "\"name\":\"GREECE\","
      "\"input_languages\":\"el\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("GS", "{"
      "\"name\":\"SOUTH GEORGIA\","
      "\"fmt\":\"%N%n%O%n%A%n%X%n%C%n%Z\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("GT", "{"
      "\"name\":\"GUATEMALA\","
      "\"input_languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%Z- %C\""
      "}"));
  region_data.insert(std::make_pair("GU", "{"
      "\"name\":\"GUAM\","
      "\"input_languages\":\"ch~en\","
      "\"fmt\":\"%N%n%O%n%A%n%C %S %Z\","
      "\"require\":\"ACSZ\","
      "\"zip_name_type\":\"zip\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("GW", "{"
      "\"name\":\"GUINEA-BISSAU\","
      "\"input_languages\":\"pt\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("GY", "{"
      "\"name\":\"GUYANA\","
      "\"input_languages\":\"en\""
      "}"));
  region_data.insert(std::make_pair("HK", "{"
      "\"name\":\"HONG KONG\","
      "\"lang\":\"zh\","
      "\"languages\":\"zh\","
      "\"input_languages\":\"en~zh\","
      "\"fmt\":\"%S%n%A%n%O%n%N\","
      "\"lfmt\":\"%N%n%O%n%A%n%S\","
      "\"require\":\"AS\","
      "\"state_name_type\":\"area\""
      "}"));
  region_data.insert(std::make_pair("HM", "{"
      "\"name\":\"HEARD AND MCDONALD ISLANDS\","
      "\"fmt\":\"%O%n%N%n%A%n%C %S %Z\""
      "}"));
  region_data.insert(std::make_pair("HN", "{"
      "\"name\":\"HONDURAS\","
      "\"input_languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%C, %S%n%Z\","
      "\"require\":\"ACS\""
      "}"));
  region_data.insert(std::make_pair("HR", "{"
      "\"name\":\"CROATIA\","
      "\"input_languages\":\"hr\","
      "\"fmt\":\"%N%n%O%n%A%nHR-%Z %C\""
      "}"));
  region_data.insert(std::make_pair("HT", "{"
      "\"name\":\"HAITI\","
      "\"input_languages\":\"fr~ht\","
      "\"fmt\":\"%N%n%O%n%A%nHT%Z %C %X\""
      "}"));
  region_data.insert(std::make_pair("HU", "{"
      "\"name\":\"HUNGARY (Rep.)\","
      "\"input_languages\":\"hu\","
      "\"fmt\":\"%N%n%O%n%C%n%A%n%Z\""
      "}"));
  region_data.insert(std::make_pair("ID", "{"
      "\"name\":\"INDONESIA\","
      "\"input_languages\":\"id~su\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z%n%S\""
      "}"));
  region_data.insert(std::make_pair("IE", "{"
      "\"name\":\"IRELAND\","
      "\"lang\":\"en\","
      "\"languages\":\"en\","
      "\"input_languages\":\"en~ga\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%S\","
      "\"state_name_type\":\"county\""
      "}"));
  region_data.insert(std::make_pair("IL", "{"
      "\"name\":\"ISRAEL\","
      "\"input_languages\":\"ar~he\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("IM", "{"
      "\"name\":\"ISLE OF MAN\","
      "\"input_languages\":\"en~gv\","
      "\"fmt\":\"%N%n%O%n%A%n%X%n%C%n%Z\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("IN", "{"
      "\"name\":\"INDIA\","
      "\"lang\":\"en\","
      "\"languages\":\"en\","
      "\"input_languages\":\"en~hi\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z%n%S\","
      "\"require\":\"ACSZ\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("IO", "{"
      "\"name\":\"BRITISH INDIAN OCEAN TERRITORY\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%X%n%C%n%Z\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("IQ", "{"
      "\"name\":\"IRAQ\","
      "\"input_languages\":\"ar\","
      "\"fmt\":\"%O%n%N%n%A%n%C, %S%n%Z\","
      "\"require\":\"ACS\""
      "}"));
  region_data.insert(std::make_pair("IS", "{"
      "\"name\":\"ICELAND\","
      "\"input_languages\":\"is\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("IT", "{"
      "\"name\":\"ITALY\","
      "\"lang\":\"it\","
      "\"languages\":\"it\","
      "\"input_languages\":\"it\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C %S\","
      "\"require\":\"ACSZ\""
      "}"));
  region_data.insert(std::make_pair("JE", "{"
      "\"name\":\"CHANNEL ISLANDS\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%X%n%C%nJERSEY%n%Z\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("JM", "{"
      "\"name\":\"JAMAICA\","
      "\"lang\":\"en\","
      "\"languages\":\"en\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%S %X\","
      "\"require\":\"ACS\","
      "\"state_name_type\":\"parish\""
      "}"));
  region_data.insert(std::make_pair("JO", "{"
      "\"name\":\"JORDAN\","
      "\"input_languages\":\"ar\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("JP", "{"
      "\"name\":\"JAPAN\","
      "\"lang\":\"ja\","
      "\"languages\":\"ja\","
      "\"input_languages\":\"ja\","
      "\"fmt\":\"\xE3\x80\x92%Z%n%S%C%n%A%n%O%n%N\","  // \xE3\x80\x92 is 〒.
      "\"lfmt\":\"%N%n%O%n%A%n%C, %S%n%Z\","
      "\"require\":\"ACSZ\","
      "\"state_name_type\":\"prefecture\""
      "}"));
  region_data.insert(std::make_pair("KE", "{"
      "\"name\":\"KENYA\","
      "\"input_languages\":\"en~sw\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%Z\""
      "}"));
  region_data.insert(std::make_pair("KG", "{"
      "\"name\":\"KYRGYZSTAN\","
      "\"input_languages\":\"ky~ru\","
      "\"fmt\":\"%Z %C %X%n%A%n%O%n%N\""
      "}"));
  region_data.insert(std::make_pair("KH", "{"
      "\"name\":\"CAMBODIA\","
      "\"input_languages\":\"km\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("KI", "{"
      "\"name\":\"KIRIBATI\","
      "\"input_languages\":\"en~gil\","
      "\"fmt\":\"%N%n%O%n%A%n%S%n%C\","
      "\"state_name_type\":\"island\""
      "}"));
  region_data.insert(std::make_pair("KM", "{"
      "\"name\":\"COMOROS\","
      "\"input_languages\":\"ar~fr~zdj\""
      "}"));
  region_data.insert(std::make_pair("KN", "{"
      "\"name\":\"SAINT KITTS AND NEVIS\","
      "\"lang\":\"en\","
      "\"languages\":\"en\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%C, %S\","
      "\"require\":\"ACS\","
      "\"state_name_type\":\"island\""
      "}"));
  region_data.insert(std::make_pair("KR", "{"
      "\"name\":\"KOREA (REP.)\","
      "\"lang\":\"ko\","
      "\"languages\":\"ko\","
      "\"input_languages\":\"ko\","
      "\"fmt\":\"%S %C%D%n%A%n%O%n%N%nSEOUL %Z\","
      "\"lfmt\":\"%N%n%O%n%A%n%D%n%C%n%S%nSEOUL %Z\","
      "\"require\":\"ACSZ\","
      "\"state_name_type\":\"do_si\""
      "}"));
  region_data.insert(std::make_pair("KW", "{"
      "\"name\":\"KUWAIT\","
      "\"input_languages\":\"ar\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("KY", "{"
      "\"name\":\"CAYMAN ISLANDS\","
      "\"lang\":\"en\","
      "\"languages\":\"en\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%S\","
      "\"require\":\"AS\","
      "\"state_name_type\":\"island\""
      "}"));
  region_data.insert(std::make_pair("KZ", "{"
      "\"name\":\"KAZAKHSTAN\","
      "\"input_languages\":\"kk~ru\","
      "\"fmt\":\"%Z%n%S%n%C%n%A%n%O%n%N\""
      "}"));
  region_data.insert(std::make_pair("LA", "{"
      "\"name\":\"LAO (PEOPLE'S DEM. REP.)\","
      "\"input_languages\":\"lo\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("LB", "{"
      "\"name\":\"LEBANON\","
      "\"input_languages\":\"ar\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("LC", "{"
      "\"name\":\"SAINT LUCIA\","
      "\"input_languages\":\"en\""
      "}"));
  region_data.insert(std::make_pair("LI", "{"
      "\"name\":\"LIECHTENSTEIN\","
      "\"input_languages\":\"de~gsw\","
      "\"fmt\":\"%O%n%N%n%A%nFL-%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("LK", "{"
      "\"name\":\"SRI LANKA\","
      "\"input_languages\":\"si~ta\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%Z\""
      "}"));
  region_data.insert(std::make_pair("LR", "{"
      "\"name\":\"LIBERIA\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C %X\""
      "}"));
  region_data.insert(std::make_pair("LS", "{"
      "\"name\":\"LESOTHO\","
      "\"input_languages\":\"en~st\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("LT", "{"
      "\"name\":\"LITHUANIA\","
      "\"input_languages\":\"lt\","
      "\"fmt\":\"%O%n%N%n%A%nLT-%Z %C\""
      "}"));
  region_data.insert(std::make_pair("LU", "{"
      "\"name\":\"LUXEMBOURG\","
      "\"input_languages\":\"de~fr~lb\","
      "\"fmt\":\"%O%n%N%n%A%nL-%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("LV", "{"
      "\"name\":\"LATVIA\","
      "\"input_languages\":\"lv\","
      "\"fmt\":\"%N%n%O%n%A%n%C, %Z\""
      "}"));
  region_data.insert(std::make_pair("LY", "{"
      "\"name\":\"LIBYA\","
      "\"input_languages\":\"ar\""
      "}"));
  region_data.insert(std::make_pair("MA", "{"
      "\"name\":\"MOROCCO\","
      "\"input_languages\":\"ar~fr~tzm\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("MC", "{"
      "\"name\":\"MONACO\","
      "\"input_languages\":\"fr\","
      "\"fmt\":\"%N%n%O%n%A%nMC-%Z %C %X\""
      "}"));
  region_data.insert(std::make_pair("MD", "{"
      "\"name\":\"Rep. MOLDOVA\","
      "\"input_languages\":\"ro\","
      "\"fmt\":\"%N%n%O%n%A%nMD-%Z %C\""
      "}"));
  region_data.insert(std::make_pair("ME", "{"
      "\"name\":\"MONTENEGRO\","
      "\"input_languages\":\"sr\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("MF", "{"
      "\"name\":\"SAINT MARTIN\","
      "\"input_languages\":\"fr\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("MG", "{"
      "\"name\":\"MADAGASCAR\","
      "\"input_languages\":\"en~fr~mg\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("MH", "{"
      "\"name\":\"MARSHALL ISLANDS\","
      "\"input_languages\":\"en~mh\","
      "\"fmt\":\"%N%n%O%n%A%n%C %S %Z\","
      "\"require\":\"ACSZ\","
      "\"zip_name_type\":\"zip\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("MK", "{"
      "\"name\":\"MACEDONIA\","
      "\"input_languages\":\"mk~sq\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("ML", "{"
      "\"name\":\"MALI\","
      "\"input_languages\":\"fr\""
      "}"));
  region_data.insert(std::make_pair("MN", "{"
      "\"name\":\"MONGOLIA\","
      "\"input_languages\":\"mn\","
      "\"fmt\":\"%N%n%O%n%A%n%S %C-%X%n%Z\""
      "}"));
  region_data.insert(std::make_pair("MO", "{"
      "\"name\":\"MACAO\","
      "\"lang\":\"zh-hant\","
      "\"languages\":\"zh-hant\","
      "\"input_languages\":\"pt~zh\","
      "\"fmt\":\"%A%n%O%n%N\","
      "\"lfmt\":\"%N%n%O%n%A\","
      "\"require\":\"A\""
      "}"));
  region_data.insert(std::make_pair("MP", "{"
      "\"name\":\"NORTHERN MARIANA ISLANDS\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%C %S %Z\","
      "\"require\":\"ACSZ\","
      "\"zip_name_type\":\"zip\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("MQ", "{"
      "\"name\":\"MARTINIQUE\","
      "\"input_languages\":\"fr\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("MR", "{"
      "\"name\":\"MAURITANIA\","
      "\"input_languages\":\"ar\""
      "}"));
  region_data.insert(std::make_pair("MS", "{"
      "\"name\":\"MONTSERRAT\","
      "\"input_languages\":\"en\""
      "}"));
  region_data.insert(std::make_pair("MT", "{"
      "\"name\":\"MALTA\","
      "\"input_languages\":\"en~mt\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("MU", "{"
      "\"name\":\"MAURITIUS\","
      "\"input_languages\":\"en~fr\","
      "\"fmt\":\"%N%n%O%n%A%n%Z%n%C\""
      "}"));
  region_data.insert(std::make_pair("MV", "{"
      "\"name\":\"MALDIVES\","
      "\"input_languages\":\"dv\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("MW", "{"
      "\"name\":\"MALAWI\","
      "\"input_languages\":\"en~ny\","
      "\"fmt\":\"%N%n%O%n%A%n%C %X\""
      "}"));
  region_data.insert(std::make_pair("MX", "{"
      "\"name\":\"MEXICO\","
      "\"lang\":\"es\","
      "\"languages\":\"es\","
      "\"input_languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C, %S\","
      "\"require\":\"ACZ\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("MY", "{"
      "\"name\":\"MALAYSIA\","
      "\"lang\":\"ms\","
      "\"languages\":\"ms\","
      "\"input_languages\":\"ms\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C, %S\","
      "\"require\":\"ACZ\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("MZ", "{"
      "\"name\":\"MOZAMBIQUE\","
      "\"input_languages\":\"pt\","
      "\"fmt\":\"%N%n%O%n%A%n%C\""
      "}"));
  region_data.insert(std::make_pair("NA", "{"
      "\"name\":\"NAMIBIA\","
      "\"input_languages\":\"af~en\""
      "}"));
  region_data.insert(std::make_pair("NC", "{"
      "\"name\":\"NEW CALEDONIA\","
      "\"input_languages\":\"fr\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("NE", "{"
      "\"name\":\"NIGER\","
      "\"input_languages\":\"fr\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("NF", "{"
      "\"name\":\"NORFOLK ISLAND\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%O%n%N%n%A%n%C %S %Z\""
      "}"));
  region_data.insert(std::make_pair("NG", "{"
      "\"name\":\"NIGERIA\","
      "\"lang\":\"fr\","
      "\"languages\":\"fr\","
      "\"input_languages\":\"efi~en~ha~ig~yo\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z%n%S\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("NI", "{"
      "\"name\":\"NICARAGUA\","
      "\"lang\":\"es\","
      "\"languages\":\"es\","
      "\"input_languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%Z%n%C, %S\","
      "\"state_name_type\":\"department\""
      "}"));
  region_data.insert(std::make_pair("NL", "{"
      "\"name\":\"NETHERLANDS\","
      "\"input_languages\":\"nl\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("NO", "{"
      "\"name\":\"NORWAY\","
      "\"input_languages\":\"nb~nn\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("NP", "{"
      "\"name\":\"NEPAL\","
      "\"input_languages\":\"ne\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("NR", "{"
      "\"name\":\"NAURU CENTRAL PACIFIC\","
      "\"lang\":\"en\","
      "\"languages\":\"en\","
      "\"input_languages\":\"en~na\","
      "\"fmt\":\"%N%n%O%n%A%n%S\","
      "\"require\":\"AS\","
      "\"state_name_type\":\"district\""
      "}"));
  region_data.insert(std::make_pair("NU", "{"
      "\"name\":\"NIUE\","
      "\"input_languages\":\"en~niu\""
      "}"));
  region_data.insert(std::make_pair("NZ", "{"
      "\"name\":\"NEW ZEALAND\","
      "\"input_languages\":\"en~mi\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("OM", "{"
      "\"name\":\"OMAN\","
      "\"input_languages\":\"ar\","
      "\"fmt\":\"%N%n%O%n%A%n%Z%n%C\""
      "}"));
  region_data.insert(std::make_pair("PA", "{"
      "\"name\":\"PANAMA (REP.)\","
      "\"input_languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%S\""
      "}"));
  region_data.insert(std::make_pair("PE", "{"
      "\"name\":\"PERU\","
      "\"input_languages\":\"es~qu\""
      "}"));
  region_data.insert(std::make_pair("PF", "{"
      "\"name\":\"FRENCH POLYNESIA\","
      "\"input_languages\":\"fr~ty\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C %S\","
      "\"require\":\"ACSZ\","
      "\"state_name_type\":\"island\""
      "}"));
  region_data.insert(std::make_pair("PG", "{"
      "\"name\":\"PAPUA NEW GUINEA\","
      "\"input_languages\":\"en~ho~tpi\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z %S\","
      "\"require\":\"ACS\""
      "}"));
  region_data.insert(std::make_pair("PH", "{"
      "\"name\":\"PHILIPPINES\","
      "\"input_languages\":\"en~fil\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C%n%S\","
      "\"require\":\"AC\""
      "}"));
  region_data.insert(std::make_pair("PK", "{"
      "\"name\":\"PAKISTAN\","
      "\"input_languages\":\"en~ur\","
      "\"fmt\":\"%N%n%O%n%A%n%C-%Z\""
      "}"));
  region_data.insert(std::make_pair("PL", "{"
      "\"name\":\"POLAND\","
      "\"input_languages\":\"pl\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("PM", "{"
      "\"name\":\"ST. PIERRE AND MIQUELON\","
      "\"input_languages\":\"fr\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("PN", "{"
      "\"name\":\"PITCAIRN\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%X%n%C%n%Z\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("PR", "{"
      "\"name\":\"PUERTO RICO\","
      "\"input_languages\":\"en~es\","
      "\"fmt\":\"%N%n%O%n%A%n%C PR %Z\","
      "\"require\":\"ACZ\","
      "\"zip_name_type\":\"zip\""
      "}"));
  region_data.insert(std::make_pair("PS", "{"
      "\"name\":\"PALESTINIAN TERRITORY\","
      "\"input_languages\":\"ar\""
      "}"));
  region_data.insert(std::make_pair("PT", "{"
      "\"name\":\"PORTUGAL\","
      "\"input_languages\":\"pt\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("PW", "{"
      "\"name\":\"PALAU\","
      "\"input_languages\":\"en~pau\","
      "\"fmt\":\"%N%n%O%n%A%n%C %S %Z\","
      "\"require\":\"ACSZ\","
      "\"zip_name_type\":\"zip\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("PY", "{"
      "\"name\":\"PARAGUAY\","
      "\"input_languages\":\"es~gn\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("QA", "{"
      "\"name\":\"QATAR\","
      "\"input_languages\":\"ar\""
      "}"));
  region_data.insert(std::make_pair("RE", "{"
      "\"name\":\"REUNION\","
      "\"input_languages\":\"fr\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("RO", "{"
      "\"name\":\"ROMANIA\","
      "\"input_languages\":\"ro\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("RS", "{"
      "\"name\":\"REPUBLIC OF SERBIA\","
      "\"input_languages\":\"sr\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("RU", "{"
      "\"name\":\"RUSSIAN FEDERATION\","
      "\"input_languages\":\"ru\","
      "\"fmt\":\"%Z %C  %n%A%n%O%n%N\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("RW", "{"
      "\"name\":\"RWANDA\","
      "\"input_languages\":\"en~fr~rw\""
      "}"));
  region_data.insert(std::make_pair("SA", "{"
      "\"name\":\"SAUDI ARABIA\","
      "\"input_languages\":\"ar\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("SB", "{"
      "\"name\":\"SOLOMON ISLANDS\","
      "\"input_languages\":\"en\""
      "}"));
  region_data.insert(std::make_pair("SC", "{"
      "\"name\":\"SEYCHELLES\","
      "\"input_languages\":\"en~fr\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%S\","
      "\"state_name_type\":\"island\""
      "}"));
  region_data.insert(std::make_pair("SE", "{"
      "\"name\":\"SWEDEN\","
      "\"input_languages\":\"sv\","
      "\"fmt\":\"%O%n%N%n%A%nSE-%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("SG", "{"
      "\"name\":\"REP. OF SINGAPORE\","
      "\"input_languages\":\"en~ms~ta~zh\","
      "\"fmt\":\"%N%n%O%n%A%nSINGAPORE %Z\","
      "\"require\":\"AZ\""
      "}"));
  region_data.insert(std::make_pair("SH", "{"
      "\"name\":\"SAINT HELENA\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%X%n%C%n%Z\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("SI", "{"
      "\"name\":\"SLOVENIA\","
      "\"input_languages\":\"sl\","
      "\"fmt\":\"%N%n%O%n%A%nSI- %Z %C\""
      "}"));
  region_data.insert(std::make_pair("SJ", "{"
      "\"name\":\"SVALBARD AND JAN MAYEN ISLANDS\","
      "\"input_languages\":\"nb\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("SK", "{"
      "\"name\":\"SLOVAKIA\","
      "\"input_languages\":\"sk\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("SL", "{"
      "\"name\":\"SIERRA LEONE\","
      "\"input_languages\":\"en\""
      "}"));
  region_data.insert(std::make_pair("SM", "{"
      "\"name\":\"SAN MARINO\","
      "\"input_languages\":\"it\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\","
      "\"require\":\"AZ\""
      "}"));
  region_data.insert(std::make_pair("SN", "{"
      "\"name\":\"SENEGAL\","
      "\"input_languages\":\"fr~wo\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("SO", "{"
      "\"name\":\"SOMALIA\","
      "\"lang\":\"so\","
      "\"languages\":\"so\","
      "\"input_languages\":\"ar~so\","
      "\"fmt\":\"%N%n%O%n%A%n%C, %S %Z\","
      "\"require\":\"ACS\""
      "}"));
  region_data.insert(std::make_pair("SR", "{"
      "\"name\":\"SURINAME\","
      "\"lang\":\"nl\","
      "\"languages\":\"nl\","
      "\"input_languages\":\"nl\","
      "\"fmt\":\"%N%n%O%n%A%n%C %X%n%S\""
      "}"));
  region_data.insert(std::make_pair("ST", "{"
      "\"name\":\"SAO TOME AND PRINCIPE\","
      "\"input_languages\":\"pt\","
      "\"fmt\":\"%N%n%O%n%A%n%C %X\""
      "}"));
  region_data.insert(std::make_pair("SV", "{"
      "\"name\":\"EL SALVADOR\","
      "\"lang\":\"es\","
      "\"languages\":\"es\","
      "\"input_languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%Z-%C%n%S\","
      "\"require\":\"ACS\""
      "}"));
  region_data.insert(std::make_pair("SZ", "{"
      "\"name\":\"SWAZILAND\","
      "\"input_languages\":\"en~ss\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%Z\""
      "}"));
  region_data.insert(std::make_pair("TC", "{"
      "\"name\":\"TURKS AND CAICOS ISLANDS\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%X%n%C%n%Z\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("TD", "{"
      "\"name\":\"CHAD\","
      "\"input_languages\":\"ar~fr\""
      "}"));
  region_data.insert(std::make_pair("TF", "{"
      "\"name\":\"FRENCH SOUTHERN TERRITORIES\""
      "}"));
  region_data.insert(std::make_pair("TG", "{"
      "\"name\":\"TOGO\","
      "\"input_languages\":\"fr\""
      "}"));
  region_data.insert(std::make_pair("TH", "{"
      "\"name\":\"THAILAND\","
      "\"lang\":\"th\","
      "\"languages\":\"th\","
      "\"input_languages\":\"th\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%S %Z\","
      "\"lfmt\":\"%N%n%O%n%A%n%C%n%S %Z\""
      "}"));
  region_data.insert(std::make_pair("TJ", "{"
      "\"name\":\"TAJIKISTAN\","
      "\"input_languages\":\"tg\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("TK", "{"
      "\"name\":\"TOKELAU\","
      "\"input_languages\":\"en~tkl\""
      "}"));
  region_data.insert(std::make_pair("TL", "{"
      "\"name\":\"TIMOR-LESTE\","
      "\"input_languages\":\"pt~tet\""
      "}"));
  region_data.insert(std::make_pair("TM", "{"
      "\"name\":\"TURKMENISTAN\","
      "\"input_languages\":\"tk\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("TN", "{"
      "\"name\":\"TUNISIA\","
      "\"input_languages\":\"ar~fr\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("TO", "{"
      "\"name\":\"TONGA\","
      "\"input_languages\":\"en~to\""
      "}"));
  region_data.insert(std::make_pair("TR", "{"
      "\"name\":\"TURKEY\","
      "\"input_languages\":\"tr\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C/%S\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("TT", "{"
      "\"name\":\"TRINIDAD AND TOBAGO\","
      "\"input_languages\":\"en\""
      "}"));
  region_data.insert(std::make_pair("TV", "{"
      "\"name\":\"TUVALU\","
      "\"lang\":\"tyv\","
      "\"languages\":\"tyv\","
      "\"input_languages\":\"en~tvl\","
      "\"fmt\":\"%N%n%O%n%A%n%X%n%C%n%S\","
      "\"state_name_type\":\"island\""
      "}"));
  region_data.insert(std::make_pair("TW", "{"
      "\"name\":\"TAIWAN\","
      "\"lang\":\"zh-hant\","
      "\"languages\":\"zh-hant\","
      "\"input_languages\":\"zh\","
      "\"fmt\":\"%Z%n%S%C%n%A%n%O%n%N\","
      "\"lfmt\":\"%N%n%O%n%A%n%C, %S %Z\","
      "\"require\":\"ACSZ\","
      "\"state_name_type\":\"county\""
      "}"));
  region_data.insert(std::make_pair("TZ", "{"
      "\"name\":\"TANZANIA (UNITED REP.)\","
      "\"input_languages\":\"en~sw\""
      "}"));
  region_data.insert(std::make_pair("UA", "{"
      "\"name\":\"UKRAINE\","
      "\"input_languages\":\"ru~uk\","
      "\"fmt\":\"%Z %C%n%A%n%O%n%N\""
      "}"));
  region_data.insert(std::make_pair("UG", "{"
      "\"name\":\"UGANDA\","
      "\"input_languages\":\"en~sw\""
      "}"));
  // NOTE: The fmt value for UM and US differs from the i18napis fmt by the
  // insertion of a comma separating city and state.
  region_data.insert(std::make_pair("UM", "{"
      "\"name\":\"UNITED STATES MINOR OUTLYING ISLANDS\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%C, %S %Z\","
      "\"require\":\"ACS\","
      "\"zip_name_type\":\"zip\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("US", "{"
      "\"name\":\"UNITED STATES\","
      "\"lang\":\"en\","
      "\"languages\":\"en\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%C, %S %Z\","
      "\"require\":\"ACSZ\","
      "\"zip_name_type\":\"zip\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("UY", "{"
      "\"name\":\"URUGUAY\","
      "\"lang\":\"es\","
      "\"languages\":\"es\","
      "\"input_languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C %S\""
      "}"));
  region_data.insert(std::make_pair("UZ", "{"
      "\"name\":\"UZBEKISTAN\","
      "\"input_languages\":\"uz\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C%n%S\""
      "}"));
  region_data.insert(std::make_pair("VA", "{"
      "\"name\":\"VATICAN\","
      "\"input_languages\":\"la\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("VC", "{"
      "\"name\":\"SAINT VINCENT AND THE GRENADINES (ANTILLES)\","
      "\"input_languages\":\"en\""
      "}"));
  region_data.insert(std::make_pair("VE", "{"
      "\"name\":\"VENEZUELA\","
      "\"lang\":\"es\","
      "\"languages\":\"es\","
      "\"input_languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z, %S\","
      "\"require\":\"ACS\""
      "}"));
  region_data.insert(std::make_pair("VG", "{"
      "\"name\":\"VIRGIN ISLANDS (BRITISH)\","
      "\"input_languages\":\"en\","
      "\"require\":\"A\""
      "}"));
  region_data.insert(std::make_pair("VI", "{"
      "\"name\":\"VIRGIN ISLANDS (U.S.)\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%C %S %Z\","
      "\"require\":\"ACSZ\","
      "\"zip_name_type\":\"zip\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("VN", "{"
      "\"name\":\"VIET NAM\","
      "\"lang\":\"vi\","
      "\"languages\":\"vi\","
      "\"input_languages\":\"vi\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%S\","
      "\"lfmt\":\"%N%n%O%n%A%n%C%n%S\","
      "\"require\":\"AC\""
      "}"));
  region_data.insert(std::make_pair("VU", "{"
      "\"name\":\"VANUATU\","
      "\"input_languages\":\"bi~en~fr\""
      "}"));
  region_data.insert(std::make_pair("WF", "{"
      "\"name\":\"WALLIS AND FUTUNA ISLANDS\","
      "\"input_languages\":\"fr\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("WS", "{"
      "\"name\":\"SAMOA\","
      "\"input_languages\":\"en~sm\""
      "}"));
  region_data.insert(std::make_pair("YE", "{"
      "\"name\":\"YEMEN\","
      "\"input_languages\":\"ar\","
      "\"require\":\"AC\""
      "}"));
  region_data.insert(std::make_pair("YT", "{"
      "\"name\":\"MAYOTTE\","
      "\"input_languages\":\"fr\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("ZA", "{"
      "\"name\":\"SOUTH AFRICA\","
      "\"input_languages\":\"af~en~nr~nso~ss~st~tn~ts~ve~xh~zu\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%Z\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("ZM", "{"
      "\"name\":\"ZAMBIA\","
      "\"input_languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\","
      "\"require\":\"AC\""
      "}"));
  region_data.insert(std::make_pair("ZW", "{"
      "\"name\":\"ZIMBABWE\","
      "\"input_languages\":\"en~nd~sn\""
      "}"));
  return region_data;
}

const std::map<std::string, std::string>& GetAllRegionData() {
  static const std::map<std::string, std::string> kRegionData(InitRegionData());
  return kRegionData;
}

struct SelectFirst {
  template <typename Pair>
  const typename Pair::first_type& operator()(const Pair& pair) const {
    return pair.first;
  }
};

std::vector<std::string> InitRegionCodes() {
  std::vector<std::string> region_codes(GetAllRegionData().size());
  std::transform(GetAllRegionData().begin(), GetAllRegionData().end(),
                 region_codes.begin(), SelectFirst());
  return region_codes;
}

}  // namespace

// static
const std::vector<std::string>& RegionDataConstants::GetRegionCodes() {
  static const std::vector<std::string> kRegionCodes(InitRegionCodes());
  return kRegionCodes;
}

// static
const std::string& RegionDataConstants::GetRegionData(
    const std::string& region_code) {
  static const std::string kEmptyString;
  std::map<std::string, std::string>::const_iterator it =
      GetAllRegionData().find(region_code);
  return it != GetAllRegionData().end() ? it->second : kEmptyString;
}

// static
const std::string& RegionDataConstants::GetDefaultRegionData() {
  static const std::string kDefaultRegionData(
      "{"
      "\"fmt\":\"%N%n%O%n%A%n%C\","
      "\"require\":\"AC\","
      "\"state_name_type\":\"province\","
      "\"zip_name_type\":\"postal\""
      "}");
  return kDefaultRegionData;
}

// static
const std::string& RegionDataConstants::GetLanguageCompactLineSeparator(
    const std::string& language_code) {
  static const std::string kEmptyString;
  static const std::string kArabicSeparator =  "، ";
  static const std::string kSpace = " ";
  static const std::string kCommaAndSpace = ", ";
  if (language_code == "ja" ||
      language_code == "zh" ||
      language_code == "zh-hant" ||
      language_code == "zh-hans") {
    return kEmptyString;
  }

  if (language_code == "ar" ||
      language_code == "cjm" ||
      language_code == "doi" ||
      language_code == "fa" ||
      language_code == "lah" ||
      language_code == "prd" ||
      language_code == "ps" ||
      language_code == "swb" ||
      language_code == "ug" ||
      language_code == "ur") {
    return kArabicSeparator;
  }

  if (language_code == "ko" ||
      language_code == "kdt" ||
      language_code == "lcp" ||
      language_code == "lwl" ||
      language_code == "th" ||
      language_code == "tts") {
    return kSpace;
  }

  return kCommaAndSpace;
}

}  // namespace addressinput
}  // namespace i18n
