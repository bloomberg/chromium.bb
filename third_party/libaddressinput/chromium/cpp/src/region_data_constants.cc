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
// The language-to-separator and country-to-language mapping is loosely based
// on:
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
      "\"fmt\":\"%N%n%O%n%A%n%Z %S\","
      "\"require\":\"AS\","
      "\"state_name_type\":\"parish\""
      "}"));
  region_data.insert(std::make_pair("AE", "{"
      "\"name\":\"UNITED ARAB EMIRATES\","
      "\"fmt\":\"%N%n%O%n%A%n%C\","
      "\"require\":\"AC\""
      "}"));
  region_data.insert(std::make_pair("AF", "{"
      "\"name\":\"AFGHANISTAN\""
      "}"));
  region_data.insert(std::make_pair("AG", "{"
      "\"name\":\"ANTIGUA AND BARBUDA\","
      "\"require\":\"A\""
      "}"));
  region_data.insert(std::make_pair("AI", "{"
      "\"name\":\"ANGUILLA\""
      "}"));
  region_data.insert(std::make_pair("AL", "{"
      "\"name\":\"ALBANIA\""
      "}"));
  region_data.insert(std::make_pair("AM", "{"
      "\"name\":\"ARMENIA\","
      "\"lang\":\"hy\","
      "\"languages\":\"hy\","
      "\"fmt\":\"%N%n%O%n%A%n%Z%n%C%n%S\""
      "}"));
  region_data.insert(std::make_pair("AN", "{"
      "\"name\":\"NETHERLANDS ANTILLES\""
      "}"));
  region_data.insert(std::make_pair("AO", "{"
      "\"name\":\"ANGOLA\""
      "}"));
  region_data.insert(std::make_pair("AQ", "{"
      "\"name\":\"ANTARCTICA\""
      "}"));
  region_data.insert(std::make_pair("AR", "{"
      "\"name\":\"ARGENTINA\","
      "\"lang\":\"es\","
      "\"languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C%n%S\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("AS", "{"
      "\"name\":\"AMERICAN SAMOA\","
      "\"fmt\":\"%N%n%O%n%A%n%C %S %Z\","
      "\"require\":\"ACSZ\","
      "\"zip_name_type\":\"zip\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("AT", "{"
      "\"name\":\"AUSTRIA\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("AU", "{"
      "\"name\":\"AUSTRALIA\","
      "\"lang\":\"en\","
      "\"languages\":\"en\","
      "\"fmt\":\"%O%n%N%n%A%n%C %S %Z\","
      "\"require\":\"ACSZ\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("AW", "{"
      "\"name\":\"ARUBA\""
      "}"));
  region_data.insert(std::make_pair("AX", "{"
      "\"name\":\"FINLAND\","
      "\"fmt\":\"%O%n%N%n%A%nAX-%Z %C%n\xC3\x85LAND\","  // \xC3\x85 is Å.
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("AZ", "{"
      "\"name\":\"AZERBAIJAN\","
      "\"fmt\":\"%N%n%O%n%A%nAZ %Z %C\""
      "}"));
  region_data.insert(std::make_pair("BA", "{"
      "\"name\":\"BOSNIA AND HERZEGOVINA\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("BB", "{"
      "\"name\":\"BARBADOS\","
      "\"state_name_type\":\"parish\""
      "}"));
  region_data.insert(std::make_pair("BD", "{"
      "\"name\":\"BANGLADESH\","
      "\"fmt\":\"%N%n%O%n%A%n%C - %Z\""
      "}"));
  region_data.insert(std::make_pair("BE", "{"
      "\"name\":\"BELGIUM\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("BF", "{"
      "\"name\":\"BURKINA FASO\","
      "\"fmt\":\"%N%n%O%n%A%n%C %X\""
      "}"));
  region_data.insert(std::make_pair("BG", "{"
      "\"name\":\"BULGARIA (REP.)\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("BH", "{"
      "\"name\":\"BAHRAIN\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("BI", "{"
      "\"name\":\"BURUNDI\""
      "}"));
  region_data.insert(std::make_pair("BJ", "{"
      "\"name\":\"BENIN\""
      "}"));
  region_data.insert(std::make_pair("BL", "{"
      "\"name\":\"SAINT BARTHELEMY\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("BM", "{"
      "\"name\":\"BERMUDA\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("BN", "{"
      "\"name\":\"BRUNEI DARUSSALAM\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("BO", "{"
      "\"name\":\"BOLIVIA\""
      "}"));
  region_data.insert(std::make_pair("BR", "{"
      "\"name\":\"BRAZIL\","
      "\"lang\":\"pt\","
      "\"languages\":\"pt\","
      "\"fmt\":\"%O%n%N%n%A%n%C-%S%n%Z\","
      "\"require\":\"ASCZ\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("BS", "{"
      "\"name\":\"BAHAMAS\","
      "\"lang\":\"en\","
      "\"languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%C, %S\","
      "\"state_name_type\":\"island\""
      "}"));
  region_data.insert(std::make_pair("BT", "{"
      "\"name\":\"BHUTAN\""
      "}"));
  region_data.insert(std::make_pair("BV", "{"
      "\"name\":\"BOUVET ISLAND\""
      "}"));
  region_data.insert(std::make_pair("BW", "{"
      "\"name\":\"BOTSWANA\""
      "}"));
  region_data.insert(std::make_pair("BY", "{"
      "\"name\":\"BELARUS\","
      "\"fmt\":\"%S%n%Z %C %X%n%A%n%O%n%N\""
      "}"));
  region_data.insert(std::make_pair("BZ", "{"
      "\"name\":\"BELIZE\""
      "}"));
  region_data.insert(std::make_pair("CA", "{"
      "\"name\":\"CANADA\","
      "\"lang\":\"en\","
      "\"languages\":\"en~fr\","
      "\"fmt\":\"%N%n%O%n%A%n%C %S %Z\","
      "\"require\":\"ACSZ\""
      "}"));
  region_data.insert(std::make_pair("CC", "{"
      "\"name\":\"COCOS (KEELING) ISLANDS\","
      "\"fmt\":\"%O%n%N%n%A%n%C %S %Z\""
      "}"));
  region_data.insert(std::make_pair("CD", "{"
      "\"name\":\"CONGO (DEM. REP.)\","
      "\"fmt\":\"%N%n%O%n%A%n%C %X\""
      "}"));
  region_data.insert(std::make_pair("CF", "{"
      "\"name\":\"CENTRAL AFRICAN REPUBLIC\""
      "}"));
  region_data.insert(std::make_pair("CG", "{"
      "\"name\":\"CONGO (REP.)\""
      "}"));
  region_data.insert(std::make_pair("CH", "{"
      "\"name\":\"SWITZERLAND\","
      "\"lang\":\"de\","
      "\"languages\":\"de~fr~it\","
      "\"fmt\":\"%O%n%N%n%A%nCH-%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("CI", "{"
      "\"name\":\"COTE D'IVOIRE\","
      "\"fmt\":\"%N%n%O%n%X %A %C %X\""
      "}"));
  region_data.insert(std::make_pair("CK", "{"
      "\"name\":\"COOK ISLANDS\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("CL", "{"
      "\"name\":\"CHILE\","
      "\"lang\":\"es\","
      "\"languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C%n%S\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("CM", "{"
      "\"name\":\"CAMEROON\""
      "}"));
  region_data.insert(std::make_pair("CN", "{"
      "\"name\":\"P.R. CHINA\","
      "\"lang\":\"zh-hans\","
      "\"languages\":\"zh-hans\","
      "\"fmt\":\"%Z%n%S%C%D%n%A%n%O%n%N\","
      "\"require\":\"ACSZ\""
      "}"));
  region_data.insert(std::make_pair("CO", "{"
      "\"name\":\"COLOMBIA\","
      "\"fmt\":\"%N%n%O%n%A%n%C, %S\""
      "}"));
  region_data.insert(std::make_pair("CR", "{"
      "\"name\":\"COSTA RICA\","
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
      "\"fmt\":\"%N%n%O%n%A%n%Z %C%n%S\","
      "\"state_name_type\":\"island\""
      "}"));
  region_data.insert(std::make_pair("CX", "{"
      "\"name\":\"CHRISTMAS ISLAND\","
      "\"fmt\":\"%O%n%N%n%A%n%C %S %Z\""
      "}"));
  region_data.insert(std::make_pair("CY", "{"
      "\"name\":\"CYPRUS\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("CZ", "{"
      "\"name\":\"CZECH REP.\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("DE", "{"
      "\"name\":\"GERMANY\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("DJ", "{"
      "\"name\":\"DJIBOUTI\""
      "}"));
  region_data.insert(std::make_pair("DK", "{"
      "\"name\":\"DENMARK\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("DM", "{"
      "\"name\":\"DOMINICA\""
      "}"));
  region_data.insert(std::make_pair("DO", "{"
      "\"name\":\"DOMINICAN REP.\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("DZ", "{"
      "\"name\":\"ALGERIA\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("EC", "{"
      "\"name\":\"ECUADOR\","
      "\"fmt\":\"%N%n%O%n%A%n%Z%n%C\""
      "}"));
  region_data.insert(std::make_pair("EE", "{"
      "\"name\":\"ESTONIA\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("EG", "{"
      "\"name\":\"EGYPT\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%S%n%Z\""
      "}"));
  region_data.insert(std::make_pair("EH", "{"
      "\"name\":\"WESTERN SAHARA\""
      "}"));
  region_data.insert(std::make_pair("ER", "{"
      "\"name\":\"ERITREA\""
      "}"));
  region_data.insert(std::make_pair("ES", "{"
      "\"name\":\"SPAIN\","
      "\"lang\":\"es\","
      "\"languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C %S\","
      "\"require\":\"ACSZ\""
      "}"));
  region_data.insert(std::make_pair("ET", "{"
      "\"name\":\"ETHIOPIA\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("FI", "{"
      "\"name\":\"FINLAND\","
      "\"fmt\":\"%O%n%N%n%A%nFI-%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("FJ", "{"
      "\"name\":\"FIJI\""
      "}"));
  region_data.insert(std::make_pair("FK", "{"
      "\"name\":\"FALKLAND ISLANDS (MALVINAS)\","
      "\"fmt\":\"%N%n%O%n%A%n%X%n%C%n%Z\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("FM", "{"
      "\"name\":\"MICRONESIA (Federated State of)\","
      "\"fmt\":\"%N%n%O%n%A%n%C %S %Z\","
      "\"require\":\"ACSZ\","
      "\"zip_name_type\":\"zip\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("FO", "{"
      "\"name\":\"FAROE ISLANDS\","
      "\"fmt\":\"%N%n%O%n%A%nFO%Z %C\""
      "}"));
  region_data.insert(std::make_pair("FR", "{"
      "\"name\":\"FRANCE\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("GA", "{"
      "\"name\":\"GABON\""
      "}"));
  region_data.insert(std::make_pair("GB", "{"
      "\"name\":\"UNITED KINGDOM\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%S%n%Z\","
      "\"require\":\"ACZ\","
      "\"state_name_type\":\"county\""
      "}"));
  region_data.insert(std::make_pair("GD", "{"
      "\"name\":\"GRENADA (WEST INDIES)\""
      "}"));
  region_data.insert(std::make_pair("GE", "{"
      "\"name\":\"GEORGIA\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("GF", "{"
      "\"name\":\"FRENCH GUIANA\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("GG", "{"
      "\"name\":\"CHANNEL ISLANDS\","
      "\"fmt\":\"%N%n%O%n%A%n%X%n%C%nGUERNSEY%n%Z\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("GH", "{"
      "\"name\":\"GHANA\""
      "}"));
  region_data.insert(std::make_pair("GI", "{"
      "\"name\":\"GIBRALTAR\","
      "\"fmt\":\"%N%n%O%n%A\","
      "\"require\":\"A\""
      "}"));
  region_data.insert(std::make_pair("GL", "{"
      "\"name\":\"GREENLAND\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("GM", "{"
      "\"name\":\"GAMBIA\""
      "}"));
  region_data.insert(std::make_pair("GN", "{"
      "\"name\":\"GUINEA\","
      "\"fmt\":\"%N%n%O%n%Z %A %C\""
      "}"));
  region_data.insert(std::make_pair("GP", "{"
      "\"name\":\"GUADELOUPE\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("GQ", "{"
      "\"name\":\"EQUATORIAL GUINEA\""
      "}"));
  region_data.insert(std::make_pair("GR", "{"
      "\"name\":\"GREECE\","
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
      "\"fmt\":\"%N%n%O%n%A%n%Z- %C\""
      "}"));
  region_data.insert(std::make_pair("GU", "{"
      "\"name\":\"GUAM\","
      "\"fmt\":\"%N%n%O%n%A%n%C %S %Z\","
      "\"require\":\"ACSZ\","
      "\"zip_name_type\":\"zip\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("GW", "{"
      "\"name\":\"GUINEA-BISSAU\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("GY", "{"
      "\"name\":\"GUYANA\""
      "}"));
  region_data.insert(std::make_pair("HK", "{"
      "\"name\":\"HONG KONG\","
      "\"lang\":\"zh\","
      "\"languages\":\"zh\","
      "\"fmt\":\"%S%n%A%n%O%n%N\","
      "\"require\":\"AS\","
      "\"state_name_type\":\"area\""
      "}"));
  region_data.insert(std::make_pair("HM", "{"
      "\"name\":\"HEARD AND MCDONALD ISLANDS\","
      "\"fmt\":\"%O%n%N%n%A%n%C %S %Z\""
      "}"));
  region_data.insert(std::make_pair("HN", "{"
      "\"name\":\"HONDURAS\","
      "\"fmt\":\"%N%n%O%n%A%n%C, %S%n%Z\","
      "\"require\":\"ACS\""
      "}"));
  region_data.insert(std::make_pair("HR", "{"
      "\"name\":\"CROATIA\","
      "\"fmt\":\"%N%n%O%n%A%nHR-%Z %C\""
      "}"));
  region_data.insert(std::make_pair("HT", "{"
      "\"name\":\"HAITI\","
      "\"fmt\":\"%N%n%O%n%A%nHT%Z %C %X\""
      "}"));
  region_data.insert(std::make_pair("HU", "{"
      "\"name\":\"HUNGARY (Rep.)\","
      "\"fmt\":\"%N%n%O%n%C%n%A%n%Z\""
      "}"));
  region_data.insert(std::make_pair("ID", "{"
      "\"name\":\"INDONESIA\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z%n%S\""
      "}"));
  region_data.insert(std::make_pair("IE", "{"
      "\"name\":\"IRELAND\","
      "\"lang\":\"en\","
      "\"languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%S\","
      "\"state_name_type\":\"county\""
      "}"));
  region_data.insert(std::make_pair("IL", "{"
      "\"name\":\"ISRAEL\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("IM", "{"
      "\"name\":\"ISLE OF MAN\","
      "\"fmt\":\"%N%n%O%n%A%n%X%n%C%n%Z\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("IN", "{"
      "\"name\":\"INDIA\","
      "\"lang\":\"en\","
      "\"languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z%n%S\","
      "\"require\":\"ACSZ\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("IO", "{"
      "\"name\":\"BRITISH INDIAN OCEAN TERRITORY\","
      "\"fmt\":\"%N%n%O%n%A%n%X%n%C%n%Z\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("IQ", "{"
      "\"name\":\"IRAQ\","
      "\"fmt\":\"%O%n%N%n%A%n%C, %S%n%Z\","
      "\"require\":\"ACS\""
      "}"));
  region_data.insert(std::make_pair("IS", "{"
      "\"name\":\"ICELAND\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("IT", "{"
      "\"name\":\"ITALY\","
      "\"lang\":\"it\","
      "\"languages\":\"it\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C %S\","
      "\"require\":\"ACSZ\""
      "}"));
  region_data.insert(std::make_pair("JE", "{"
      "\"name\":\"CHANNEL ISLANDS\","
      "\"fmt\":\"%N%n%O%n%A%n%X%n%C%nJERSEY%n%Z\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("JM", "{"
      "\"name\":\"JAMAICA\","
      "\"lang\":\"en\","
      "\"languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%S %X\","
      "\"require\":\"ACS\","
      "\"state_name_type\":\"parish\""
      "}"));
  region_data.insert(std::make_pair("JO", "{"
      "\"name\":\"JORDAN\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("JP", "{"
      "\"name\":\"JAPAN\","
      "\"lang\":\"ja\","
      "\"languages\":\"ja\","
      "\"fmt\":\"\xE3\x80\x92%Z%n%S%C%n%A%n%O%n%N\","  // \xE3\x80\x92 is 〒.
      "\"require\":\"ACSZ\","
      "\"state_name_type\":\"prefecture\""
      "}"));
  region_data.insert(std::make_pair("KE", "{"
      "\"name\":\"KENYA\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%Z\""
      "}"));
  region_data.insert(std::make_pair("KG", "{"
      "\"name\":\"KYRGYZSTAN\","
      "\"fmt\":\"%Z %C %X%n%A%n%O%n%N\""
      "}"));
  region_data.insert(std::make_pair("KH", "{"
      "\"name\":\"CAMBODIA\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("KI", "{"
      "\"name\":\"KIRIBATI\","
      "\"fmt\":\"%N%n%O%n%A%n%S%n%C\","
      "\"state_name_type\":\"island\""
      "}"));
  region_data.insert(std::make_pair("KM", "{"
      "\"name\":\"COMOROS\""
      "}"));
  region_data.insert(std::make_pair("KN", "{"
      "\"name\":\"SAINT KITTS AND NEVIS\","
      "\"lang\":\"en\","
      "\"languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%C, %S\","
      "\"require\":\"ACS\","
      "\"state_name_type\":\"island\""
      "}"));
  region_data.insert(std::make_pair("KR", "{"
      "\"name\":\"KOREA (REP.)\","
      "\"lang\":\"ko\","
      "\"languages\":\"ko\","
      "\"fmt\":\"%S %C%D%n%A%n%O%n%N%nSEOUL %Z\","
      "\"require\":\"ACSZ\","
      "\"state_name_type\":\"do_si\""
      "}"));
  region_data.insert(std::make_pair("KW", "{"
      "\"name\":\"KUWAIT\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("KY", "{"
      "\"name\":\"CAYMAN ISLANDS\","
      "\"lang\":\"en\","
      "\"languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%S\","
      "\"require\":\"AS\","
      "\"state_name_type\":\"island\""
      "}"));
  region_data.insert(std::make_pair("KZ", "{"
      "\"name\":\"KAZAKHSTAN\","
      "\"fmt\":\"%Z%n%S%n%C%n%A%n%O%n%N\""
      "}"));
  region_data.insert(std::make_pair("LA", "{"
      "\"name\":\"LAO (PEOPLE'S DEM. REP.)\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("LB", "{"
      "\"name\":\"LEBANON\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("LC", "{"
      "\"name\":\"SAINT LUCIA\""
      "}"));
  region_data.insert(std::make_pair("LI", "{"
      "\"name\":\"LIECHTENSTEIN\","
      "\"fmt\":\"%O%n%N%n%A%nFL-%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("LK", "{"
      "\"name\":\"SRI LANKA\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%Z\""
      "}"));
  region_data.insert(std::make_pair("LR", "{"
      "\"name\":\"LIBERIA\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C %X\""
      "}"));
  region_data.insert(std::make_pair("LS", "{"
      "\"name\":\"LESOTHO\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("LT", "{"
      "\"name\":\"LITHUANIA\","
      "\"fmt\":\"%O%n%N%n%A%nLT-%Z %C\""
      "}"));
  region_data.insert(std::make_pair("LU", "{"
      "\"name\":\"LUXEMBOURG\","
      "\"fmt\":\"%O%n%N%n%A%nL-%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("LV", "{"
      "\"name\":\"LATVIA\","
      "\"fmt\":\"%N%n%O%n%A%n%C, %Z\""
      "}"));
  region_data.insert(std::make_pair("LY", "{"
      "\"name\":\"LIBYA\""
      "}"));
  region_data.insert(std::make_pair("MA", "{"
      "\"name\":\"MOROCCO\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("MC", "{"
      "\"name\":\"MONACO\","
      "\"fmt\":\"%N%n%O%n%A%nMC-%Z %C %X\""
      "}"));
  region_data.insert(std::make_pair("MD", "{"
      "\"name\":\"Rep. MOLDOVA\","
      "\"fmt\":\"%N%n%O%n%A%nMD-%Z %C\""
      "}"));
  region_data.insert(std::make_pair("ME", "{"
      "\"name\":\"MONTENEGRO\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("MF", "{"
      "\"name\":\"SAINT MARTIN\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("MG", "{"
      "\"name\":\"MADAGASCAR\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("MH", "{"
      "\"name\":\"MARSHALL ISLANDS\","
      "\"fmt\":\"%N%n%O%n%A%n%C %S %Z\","
      "\"require\":\"ACSZ\","
      "\"zip_name_type\":\"zip\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("MK", "{"
      "\"name\":\"MACEDONIA\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("ML", "{"
      "\"name\":\"MALI\""
      "}"));
  region_data.insert(std::make_pair("MN", "{"
      "\"name\":\"MONGOLIA\","
      "\"fmt\":\"%N%n%O%n%A%n%S %C-%X%n%Z\""
      "}"));
  region_data.insert(std::make_pair("MO", "{"
      "\"name\":\"MACAO\","
      "\"lang\":\"zh-hant\","
      "\"languages\":\"zh-hant\","
      "\"fmt\":\"%A%n%O%n%N\","
      "\"require\":\"A\""
      "}"));
  region_data.insert(std::make_pair("MP", "{"
      "\"name\":\"NORTHERN MARIANA ISLANDS\","
      "\"fmt\":\"%N%n%O%n%A%n%C %S %Z\","
      "\"require\":\"ACSZ\","
      "\"zip_name_type\":\"zip\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("MQ", "{"
      "\"name\":\"MARTINIQUE\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("MR", "{"
      "\"name\":\"MAURITANIA\""
      "}"));
  region_data.insert(std::make_pair("MS", "{"
      "\"name\":\"MONTSERRAT\""
      "}"));
  region_data.insert(std::make_pair("MT", "{"
      "\"name\":\"MALTA\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("MU", "{"
      "\"name\":\"MAURITIUS\","
      "\"fmt\":\"%N%n%O%n%A%n%Z%n%C\""
      "}"));
  region_data.insert(std::make_pair("MV", "{"
      "\"name\":\"MALDIVES\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("MW", "{"
      "\"name\":\"MALAWI\","
      "\"fmt\":\"%N%n%O%n%A%n%C %X\""
      "}"));
  region_data.insert(std::make_pair("MX", "{"
      "\"name\":\"MEXICO\","
      "\"lang\":\"es\","
      "\"languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C, %S\","
      "\"require\":\"ACZ\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("MY", "{"
      "\"name\":\"MALAYSIA\","
      "\"lang\":\"ms\","
      "\"languages\":\"ms\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C, %S\","
      "\"require\":\"ACZ\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("MZ", "{"
      "\"name\":\"MOZAMBIQUE\","
      "\"fmt\":\"%N%n%O%n%A%n%C\""
      "}"));
  region_data.insert(std::make_pair("NA", "{"
      "\"name\":\"NAMIBIA\""
      "}"));
  region_data.insert(std::make_pair("NC", "{"
      "\"name\":\"NEW CALEDONIA\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("NE", "{"
      "\"name\":\"NIGER\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("NF", "{"
      "\"name\":\"NORFOLK ISLAND\","
      "\"fmt\":\"%O%n%N%n%A%n%C %S %Z\""
      "}"));
  region_data.insert(std::make_pair("NG", "{"
      "\"name\":\"NIGERIA\","
      "\"lang\":\"fr\","
      "\"languages\":\"fr\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z%n%S\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("NI", "{"
      "\"name\":\"NICARAGUA\","
      "\"lang\":\"es\","
      "\"languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%Z%n%C, %S\","
      "\"state_name_type\":\"department\""
      "}"));
  region_data.insert(std::make_pair("NL", "{"
      "\"name\":\"NETHERLANDS\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("NO", "{"
      "\"name\":\"NORWAY\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("NP", "{"
      "\"name\":\"NEPAL\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("NR", "{"
      "\"name\":\"NAURU CENTRAL PACIFIC\","
      "\"lang\":\"en\","
      "\"languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%S\","
      "\"require\":\"AS\","
      "\"state_name_type\":\"district\""
      "}"));
  region_data.insert(std::make_pair("NU", "{"
      "\"name\":\"NIUE\""
      "}"));
  region_data.insert(std::make_pair("NZ", "{"
      "\"name\":\"NEW ZEALAND\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("OM", "{"
      "\"name\":\"OMAN\","
      "\"fmt\":\"%N%n%O%n%A%n%Z%n%C\""
      "}"));
  region_data.insert(std::make_pair("PA", "{"
      "\"name\":\"PANAMA (REP.)\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%S\""
      "}"));
  region_data.insert(std::make_pair("PE", "{"
      "\"name\":\"PERU\""
      "}"));
  region_data.insert(std::make_pair("PF", "{"
      "\"name\":\"FRENCH POLYNESIA\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C %S\","
      "\"require\":\"ACSZ\","
      "\"state_name_type\":\"island\""
      "}"));
  region_data.insert(std::make_pair("PG", "{"
      "\"name\":\"PAPUA NEW GUINEA\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z %S\","
      "\"require\":\"ACS\""
      "}"));
  region_data.insert(std::make_pair("PH", "{"
      "\"name\":\"PHILIPPINES\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C%n%S\","
      "\"require\":\"AC\""
      "}"));
  region_data.insert(std::make_pair("PK", "{"
      "\"name\":\"PAKISTAN\","
      "\"fmt\":\"%N%n%O%n%A%n%C-%Z\""
      "}"));
  region_data.insert(std::make_pair("PL", "{"
      "\"name\":\"POLAND\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("PM", "{"
      "\"name\":\"ST. PIERRE AND MIQUELON\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("PN", "{"
      "\"name\":\"PITCAIRN\","
      "\"fmt\":\"%N%n%O%n%A%n%X%n%C%n%Z\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("PR", "{"
      "\"name\":\"PUERTO RICO\","
      "\"fmt\":\"%N%n%O%n%A%n%C PR %Z\","
      "\"require\":\"ACZ\","
      "\"zip_name_type\":\"zip\""
      "}"));
  region_data.insert(std::make_pair("PS", "{"
      "\"name\":\"PALESTINIAN TERRITORY\""
      "}"));
  region_data.insert(std::make_pair("PT", "{"
      "\"name\":\"PORTUGAL\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("PW", "{"
      "\"name\":\"PALAU\","
      "\"fmt\":\"%N%n%O%n%A%n%C %S %Z\","
      "\"require\":\"ACSZ\","
      "\"zip_name_type\":\"zip\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("PY", "{"
      "\"name\":\"PARAGUAY\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("QA", "{"
      "\"name\":\"QATAR\""
      "}"));
  region_data.insert(std::make_pair("RE", "{"
      "\"name\":\"REUNION\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("RO", "{"
      "\"name\":\"ROMANIA\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("RS", "{"
      "\"name\":\"REPUBLIC OF SERBIA\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("RU", "{"
      "\"name\":\"RUSSIAN FEDERATION\","
      "\"fmt\":\"%Z %C  %n%A%n%O%n%N\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("RW", "{"
      "\"name\":\"RWANDA\""
      "}"));
  region_data.insert(std::make_pair("SA", "{"
      "\"name\":\"SAUDI ARABIA\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z\""
      "}"));
  region_data.insert(std::make_pair("SB", "{"
      "\"name\":\"SOLOMON ISLANDS\""
      "}"));
  region_data.insert(std::make_pair("SC", "{"
      "\"name\":\"SEYCHELLES\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%S\","
      "\"state_name_type\":\"island\""
      "}"));
  region_data.insert(std::make_pair("SE", "{"
      "\"name\":\"SWEDEN\","
      "\"fmt\":\"%O%n%N%n%A%nSE-%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("SG", "{"
      "\"name\":\"REP. OF SINGAPORE\","
      "\"fmt\":\"%N%n%O%n%A%nSINGAPORE %Z\","
      "\"require\":\"AZ\""
      "}"));
  region_data.insert(std::make_pair("SH", "{"
      "\"name\":\"SAINT HELENA\","
      "\"fmt\":\"%N%n%O%n%A%n%X%n%C%n%Z\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("SI", "{"
      "\"name\":\"SLOVENIA\","
      "\"fmt\":\"%N%n%O%n%A%nSI- %Z %C\""
      "}"));
  region_data.insert(std::make_pair("SJ", "{"
      "\"name\":\"SVALBARD AND JAN MAYEN ISLANDS\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("SK", "{"
      "\"name\":\"SLOVAKIA\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("SL", "{"
      "\"name\":\"SIERRA LEONE\""
      "}"));
  region_data.insert(std::make_pair("SM", "{"
      "\"name\":\"SAN MARINO\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\","
      "\"require\":\"AZ\""
      "}"));
  region_data.insert(std::make_pair("SN", "{"
      "\"name\":\"SENEGAL\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("SO", "{"
      "\"name\":\"SOMALIA\","
      "\"lang\":\"so\","
      "\"languages\":\"so\","
      "\"fmt\":\"%N%n%O%n%A%n%C, %S %Z\","
      "\"require\":\"ACS\""
      "}"));
  region_data.insert(std::make_pair("SR", "{"
      "\"name\":\"SURINAME\","
      "\"lang\":\"nl\","
      "\"languages\":\"nl\","
      "\"fmt\":\"%N%n%O%n%A%n%C %X%n%S\""
      "}"));
  region_data.insert(std::make_pair("ST", "{"
      "\"name\":\"SAO TOME AND PRINCIPE\","
      "\"fmt\":\"%N%n%O%n%A%n%C %X\""
      "}"));
  region_data.insert(std::make_pair("SV", "{"
      "\"name\":\"EL SALVADOR\","
      "\"lang\":\"es\","
      "\"languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%Z-%C%n%S\","
      "\"require\":\"ACS\""
      "}"));
  region_data.insert(std::make_pair("SZ", "{"
      "\"name\":\"SWAZILAND\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%Z\""
      "}"));
  region_data.insert(std::make_pair("TC", "{"
      "\"name\":\"TURKS AND CAICOS ISLANDS\","
      "\"fmt\":\"%N%n%O%n%A%n%X%n%C%n%Z\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("TD", "{"
      "\"name\":\"CHAD\""
      "}"));
  region_data.insert(std::make_pair("TF", "{"
      "\"name\":\"FRENCH SOUTHERN TERRITORIES\""
      "}"));
  region_data.insert(std::make_pair("TG", "{"
      "\"name\":\"TOGO\""
      "}"));
  region_data.insert(std::make_pair("TH", "{"
      "\"name\":\"THAILAND\","
      "\"lang\":\"th\","
      "\"languages\":\"th\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%S %Z\""
      "}"));
  region_data.insert(std::make_pair("TJ", "{"
      "\"name\":\"TAJIKISTAN\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("TK", "{"
      "\"name\":\"TOKELAU\""
      "}"));
  region_data.insert(std::make_pair("TL", "{"
      "\"name\":\"TIMOR-LESTE\""
      "}"));
  region_data.insert(std::make_pair("TM", "{"
      "\"name\":\"TURKMENISTAN\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("TN", "{"
      "\"name\":\"TUNISIA\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("TO", "{"
      "\"name\":\"TONGA\""
      "}"));
  region_data.insert(std::make_pair("TR", "{"
      "\"name\":\"TURKEY\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C/%S\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("TT", "{"
      "\"name\":\"TRINIDAD AND TOBAGO\""
      "}"));
  region_data.insert(std::make_pair("TV", "{"
      "\"name\":\"TUVALU\","
      "\"lang\":\"tyv\","
      "\"languages\":\"tyv\","
      "\"fmt\":\"%N%n%O%n%A%n%X%n%C%n%S\","
      "\"state_name_type\":\"island\""
      "}"));
  region_data.insert(std::make_pair("TW", "{"
      "\"name\":\"TAIWAN\","
      "\"lang\":\"zh-hant\","
      "\"languages\":\"zh-hant\","
      "\"fmt\":\"%Z%n%S%C%n%A%n%O%n%N\","
      "\"require\":\"ACSZ\","
      "\"state_name_type\":\"county\""
      "}"));
  region_data.insert(std::make_pair("TZ", "{"
      "\"name\":\"TANZANIA (UNITED REP.)\""
      "}"));
  region_data.insert(std::make_pair("UA", "{"
      "\"name\":\"UKRAINE\","
      "\"fmt\":\"%Z %C%n%A%n%O%n%N\""
      "}"));
  region_data.insert(std::make_pair("UG", "{"
      "\"name\":\"UGANDA\""
      "}"));
  // NOTE: The fmt value for UM and US differs from the i18napis fmt by the
  // insertion of a comma separating city and state.
  region_data.insert(std::make_pair("UM", "{"
      "\"name\":\"UNITED STATES MINOR OUTLYING ISLANDS\","
      "\"fmt\":\"%N%n%O%n%A%n%C, %S %Z\","
      "\"require\":\"ACS\","
      "\"zip_name_type\":\"zip\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("US", "{"
      "\"name\":\"UNITED STATES\","
      "\"lang\":\"en\","
      "\"languages\":\"en\","
      "\"fmt\":\"%N%n%O%n%A%n%C, %S %Z\","
      "\"require\":\"ACSZ\","
      "\"zip_name_type\":\"zip\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("UY", "{"
      "\"name\":\"URUGUAY\","
      "\"lang\":\"es\","
      "\"languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C %S\""
      "}"));
  region_data.insert(std::make_pair("UZ", "{"
      "\"name\":\"UZBEKISTAN\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C%n%S\""
      "}"));
  region_data.insert(std::make_pair("VA", "{"
      "\"name\":\"VATICAN\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\""
      "}"));
  region_data.insert(std::make_pair("VC", "{"
      "\"name\":\"SAINT VINCENT AND THE GRENADINES (ANTILLES)\""
      "}"));
  region_data.insert(std::make_pair("VE", "{"
      "\"name\":\"VENEZUELA\","
      "\"lang\":\"es\","
      "\"languages\":\"es\","
      "\"fmt\":\"%N%n%O%n%A%n%C %Z, %S\","
      "\"require\":\"ACS\""
      "}"));
  region_data.insert(std::make_pair("VG", "{"
      "\"name\":\"VIRGIN ISLANDS (BRITISH)\","
      "\"require\":\"A\""
      "}"));
  region_data.insert(std::make_pair("VI", "{"
      "\"name\":\"VIRGIN ISLANDS (U.S.)\","
      "\"fmt\":\"%N%n%O%n%A%n%C %S %Z\","
      "\"require\":\"ACSZ\","
      "\"zip_name_type\":\"zip\","
      "\"state_name_type\":\"state\""
      "}"));
  region_data.insert(std::make_pair("VN", "{"
      "\"name\":\"VIET NAM\","
      "\"lang\":\"vi\","
      "\"languages\":\"vi\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%S\","
      "\"require\":\"AC\""
      "}"));
  region_data.insert(std::make_pair("VU", "{"
      "\"name\":\"VANUATU\""
      "}"));
  region_data.insert(std::make_pair("WF", "{"
      "\"name\":\"WALLIS AND FUTUNA ISLANDS\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("WS", "{"
      "\"name\":\"SAMOA\""
      "}"));
  region_data.insert(std::make_pair("YE", "{"
      "\"name\":\"YEMEN\","
      "\"require\":\"AC\""
      "}"));
  region_data.insert(std::make_pair("YT", "{"
      "\"name\":\"MAYOTTE\","
      "\"fmt\":\"%O%n%N%n%A%n%Z %C %X\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("ZA", "{"
      "\"name\":\"SOUTH AFRICA\","
      "\"fmt\":\"%N%n%O%n%A%n%C%n%Z\","
      "\"require\":\"ACZ\""
      "}"));
  region_data.insert(std::make_pair("ZM", "{"
      "\"name\":\"ZAMBIA\","
      "\"fmt\":\"%N%n%O%n%A%n%Z %C\","
      "\"require\":\"AC\""
      "}"));
  region_data.insert(std::make_pair("ZW", "{"
      "\"name\":\"ZIMBABWE\""
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
