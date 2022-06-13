// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains UTF16 strings that we want as char16_t arrays.

#include "components/autofill/core/browser/autofill_regex_constants.h"

namespace autofill {

/////////////////////////////////////////////////////////////////////////////
// address_field.cc
/////////////////////////////////////////////////////////////////////////////
const char16_t kAttentionIgnoredRe[] = u"attention|attn";
const char16_t kRegionIgnoredRe[] =
    u"province|region|other"
    u"|provincia"       // es
    u"|bairro|suburb";  // pt-BR, pt-PT
const char16_t kAddressNameIgnoredRe[] =
    u"address.*nickname|address.*label"
    u"|adres ([İi]sim|başlığı|adı)"  // tr
    u"|identificação do endereço"    // pt-BR, pt-PT
    u"|(label|judul|nama) alamat";   // id
const char16_t kCompanyRe[] =
    u"company|business|organization|organisation"
    u"|(?<!con)firma|firmenname"  // de-DE
    u"|empresa"                   // es
    u"|societe|société"           // fr-FR
    u"|ragione.?sociale"          // it-IT
    u"|会社"                      // ja-JP
    u"|название.?компании"        // ru
    u"|单位|公司"                 // zh-CN
    u"|شرکت"                      // fa
    u"|회사|직장"                 // ko-KR
    u"|(nama.?)?perusahaan";      // id
const char16_t kStreetNameRe[] =
    u"stra(ss|ß)e"              // de
    u"|street"                  // en
    u"|улица|название.?улицы"   // ru
    u"|rua|avenida"             // pt-PT, pt-BR
    u"|((?<!do |de )endereço)"  // pt-BR
    u"|calle";                  // es-MX
const char16_t kHouseNumberRe[] =
    u"(house.?|street.?|^)number"              // en
    u"|(haus|^)(nummer|nr\\.?)"                // de
    u"|^\\*?.?número(.?\\*?$| da residência)"  // pt-BR, pt-PT
    u"|дом|номер.?дома"                        // ru
    u"|exterior";                              // es-MX
const char16_t kApartmentNumberRe[] =
    u"apartment"                      // en
    u"|interior"                      // es-MX
    u"|n(u|ú)mero.*app?art(a|e)ment"  // es,fr,it
    u"|Wohnung"                       // de
    u"|квартир";                      // ru
const char16_t kAddressLine1Re[] =
    u"^address$|address[_-]?line(one)?|address1|addr1|street"
    u"|(?:shipping|billing)address$"
    u"|strasse|straße|hausnummer|housenumber"  // de-DE
    u"|house.?name"                            // en-GB
    u"|direccion|dirección"                    // es
    u"|adresse"                                // fr-FR
    u"|indirizzo"                              // it-IT
    u"|^住所$|住所1"                           // ja-JP
    u"|morada|((?<!do |de )endereço)"          // pt-BR, pt-PT
    u"|Адрес"                                  // ru
    u"|地址"                                   // zh-CN
    u"|(\\b|_)adres(?! tarifi)(\\b|_)"         // tr
    u"|^주소.?$|주소.?1"                       // ko-KR
    u"|^alamat";                               // id
const char16_t kAddressLine1LabelRe[] =
    u"(^\\W*address)"
    u"|(address\\W*$)"
    u"|(?:shipping|billing|mailing|pick.?up|drop.?off|delivery|sender|postal|"
    u"recipient|home|work|office|school|business|mail)[\\s\\-]+address"
    u"|address\\s+(of|for|to|from)"
    u"|adresse"                         // fr-FR
    u"|indirizzo"                       // it-IT
    u"|住所"                            // ja-JP
    u"|地址"                            // zh-CN
    u"|(\\b|_)adres(?! tarifi)(\\b|_)"  // tr
    u"|주소"                            // ko-KR
    u"|^alamat"                         // id
    // Should contain street and any other address component, in any order
    u"|street.*(house|building|apartment|floor)"  // en
    u"|(house|building|apartment|floor).*street"
    u"|(sokak|cadde).*(apartman|bina|daire|mahalle)"  // tr
    u"|(apartman|bina|daire|mahalle).*(sokak|cadde)"
    u"|улиц.*(дом|корпус|квартир|этаж)|(дом|корпус|квартир|этаж).*улиц";  // ru
const char16_t kAddressLine2Re[] =
    u"address[_-]?line(2|two)|address2|addr2|street|suite|unit"
    u"|adresszusatz|ergänzende.?angaben"        // de-DE
    u"|direccion2|colonia|adicional"            // es
    u"|addresssuppl|complementnom|appartement"  // fr-FR
    u"|indirizzo2"                              // it-IT
    u"|住所2"                                   // ja-JP
    u"|complemento|addrcomplement"              // pt-BR, pt-PT
    u"|Улица"                                   // ru
    u"|地址2"                                   // zh-CN
    u"|주소.?2";                                // ko-KR
const char16_t kAddressLine2LabelRe[] =
    u"address|line"
    u"|adresse"    // fr-FR
    u"|indirizzo"  // it-IT
    u"|地址"       // zh-CN
    u"|주소";      // ko-KR
const char16_t kAddressLinesExtraRe[] =
    u"address.*line[3-9]|address[3-9]|addr[3-9]|street|line[3-9]"
    u"|municipio"           // es
    u"|batiment|residence"  // fr-FR
    u"|indirizzo[3-9]";     // it-IT
const char16_t kAddressLookupRe[] = u"lookup";
const char16_t kCountryRe[] =
    u"country|countries"
    u"|país|pais"                         // es
    u"|(\\b|_)land(\\b|_)(?!.*(mark.*))"  // de-DE landmark is a type in india.
    u"|(?<!(入|出))国"                    // ja-JP
    u"|国家"                              // zh-CN
    u"|국가|나라"                         // ko-KR
    u"|(\\b|_)(ülke|ulce|ulke)(\\b|_)"    // tr
    u"|کشور"                              // fa
    u"|negara";                           // id
const char16_t kCountryLocationRe[] = u"location";
const char16_t kZipCodeRe[] =
    u"((?<!\\.))zip"  // .zip indicates a file extension
    u"|postal|post.*code|pcode"
    u"|pin.?code"                    // en-IN
    u"|postleitzahl"                 // de-DE
    u"|\\bcp\\b"                     // es
    u"|\\bcdp\\b"                    // fr-FR
    u"|\\bcap\\b"                    // it-IT
    u"|郵便番号"                     // ja-JP
    u"|codigo|codpos|\\bcep\\b"      // pt-BR, pt-PT
    u"|Почтовый.?Индекс"             // ru
    u"|पिन.?कोड"                     // hi
    u"|പിന്‍കോഡ്"  // ml
    u"|邮政编码|邮编"                // zh-CN
    u"|郵遞區號"                     // zh-TW
    u"|(\\b|_)posta kodu(\\b|_)"     // tr
    u"|우편.?번호"                   // ko-KR
    u"|kode.?pos";                   // id
const char16_t kZip4Re[] =
    u"((?<!\\.))zip"  // .zip indicates a file extension
    u"|^-$|post2"
    u"|codpos2";  // pt-BR, pt-PT
const char16_t kDependentLocalityRe[] =
    u"neighbo(u)?rhood"  // en
    u"|bairro"           // pt-BR, pt-PT
    u"|mahalle|köy"      // tr
    u"|kecamatan";       // id
const char16_t kCityRe[] =
    u"city|town"
    u"|\\bort\\b|stadt"                                  // de-DE
    u"|suburb"                                           // en-AU
    u"|ciudad|provincia|localidad|poblacion"             // es
    u"|ville|commune"                                    // fr-FR
    u"|localita"                                         // it-IT
    u"|市区町村"                                         // ja-JP
    u"|cidade|município"                                 // pt-BR, pt-PT
    u"|Город|Насел(е|ё)нный.?пункт"                      // ru
    u"|市"                                               // zh-CN
    u"|分區"                                             // zh-TW
    u"|شهر"                                              // fa
    u"|शहर"                                              // hi for city
    u"|ग्राम|गाँव"                                         // hi for village
    u"|നഗരം|ഗ്രാമം"                                       // ml for town|village
    u"|((\\b|_|\\*)([İii̇]l[cç]e(miz|niz)?)(\\b|_|\\*))"  // tr
    u"|^시[^도·・]|시[·・]?군[·・]?구"                   // ko-KR
    u"|kota|kabupaten";                                  // id
const char16_t kStateRe[] =
    u"(?<!(united|hist|history).?)state|county|region|province"
    u"|county|principality"  // en-UK
    u"|都道府県"             // ja-JP
    u"|estado|provincia"     // pt-BR, pt-PT
    u"|область"              // ru
    u"|省"                   // zh-CN
    u"|地區"                 // zh-TW
    u"|സംസ്ഥാനം"              // ml
    u"|استان"                // fa
    u"|राज्य"                 // hi
    u"|((\\b|_|\\*)(eyalet|[şs]ehir|[İii̇]l(imiz)?|kent)(\\b|_|\\*))"  // tr
    u"|^시[·・]?도"                                                   // ko-KR
    u"|provinci";                                                     // id

/////////////////////////////////////////////////////////////////////////////
// search_field.cc
/////////////////////////////////////////////////////////////////////////////
const char16_t kSearchTermRe[] =
    u"^q$"
    u"|search"
    u"|query"
    u"|qry"
    u"|suche.*"              // de-DE
    u"|搜索"                 // zh-CN zh-TW
    u"|探す|検索"            // ja-JP to search
    u"|recherch.*"           // fr-FR
    u"|busca"                // pt-BR, pt-PT
    u"|جستجو"                // fa
    u"|искать|найти|поиск";  // ru

/////////////////////////////////////////////////////////////////////////////
// field_price.cc
/////////////////////////////////////////////////////////////////////////////
const char16_t kPriceRe[] =
    u"\\bprice\\b|\\brate\\b|\\bcost\\b"
    u"|قیمة‎|سعر‎"                          // ar
    u"|قیمت"                                           // fa
    u"|\\bprix\\b|\\bcoût\\b|\\bcout\\b|\\btarif\\b";  // fr-CA

/////////////////////////////////////////////////////////////////////////////
// credit_card_field.cc
/////////////////////////////////////////////////////////////////////////////
const char16_t kNameOnCardRe[] =
    u"card.?(?:holder|owner)|name.*(\\b)?on(\\b)?.*card"
    u"|(?:card|cc).?name|cc.?full.?name"
    u"|karteninhaber"                   // de-DE
    u"|nombre.*tarjeta"                 // es
    u"|nom.*carte"                      // fr-FR
    u"|nome.*cart"                      // it-IT
    u"|名前"                            // ja-JP
    u"|Имя.*карты"                      // ru
    u"|nama.*kartu"                     // id
    u"|信用卡开户名|开户名|持卡人姓名"  // zh-CN
    u"|持卡人姓名";                     // zh-TW
const char16_t kNameOnCardContextualRe[] = u"name";
const char16_t kCardNumberRe[] =
    u"(add)?(?:card|cc|acct).?(?:number|#|no|num|field|pan)"
    u"|(?<!telefon|haus|person|fødsels|kunden)nummer"  // de-DE, sv-SE, no
    u"|カード番号"                                     // ja-JP
    u"|Номер.*карты"                                   // ru
    u"|no.*kartu"                                      // id
    u"|信用卡号|信用卡号码"                            // zh-CN
    u"|信用卡卡號"                                     // zh-TW
    u"|카드"                                           // ko-KR
    // es/pt/fr
    u"|(numero|número|numéro)(?!.*(document|fono|phone|réservation))";

const char16_t kCardCvcRe[] =
    u"verification|card.?identification|security.?code|card.?code"
    u"|security.?value"
    u"|security.?number|card.?pin|c-v-v"
    u"|(cvn|cvv|cvc|csc|cvd|cid|ccv)(field)?"
    u"|\\bcid\\b";

// "Expiration date" is the most common label here, but some pages have
// "Expires", "exp. date" or "exp. month" and "exp. year".  We also look
// for the field names ccmonth and ccyear, which appear on at least 4 of
// our test pages.

// On at least one page (The China Shop2.html) we find only the labels
// "month" and "year".  So for now we match these words directly; we'll
// see if this turns out to be too general.

// Toolbar Bug 51451: indeed, simply matching "month" is too general for
//   https://rps.fidelity.com/ftgw/rps/RtlCust/CreatePIN/Init.
// Instead, we match only words beginning with "month".
const char16_t kExpirationMonthRe[] =
    u"expir|exp.*mo|exp.*date|ccmonth|cardmonth|addmonth"
    u"|gueltig|gültig|monat"         // de-DE
    u"|fecha"                        // es
    u"|date.*exp"                    // fr-FR
    u"|scadenza"                     // it-IT
    u"|有効期限"                     // ja-JP
    u"|validade"                     // pt-BR, pt-PT
    u"|Срок действия карты"          // ru
    u"|masa berlaku|berlaku hingga"  // id
    u"|月";                          // zh-CN
const char16_t kExpirationYearRe[] =
    u"exp|^/|(add)?year"
    u"|ablaufdatum|gueltig|gültig|jahr"  // de-DE
    u"|fecha"                            // es
    u"|scadenza"                         // it-IT
    u"|有効期限"                         // ja-JP
    u"|validade"                         // pt-BR, pt-PT
    u"|Срок действия карты"              // ru
    u"|masa berlaku|berlaku hingga"      // id
    u"|年|有效期";                       // zh-CN

// Used to match a expiration date field with a two digit year.
// The following conditions must be met:
//  - Exactly two adjacent y's.
//  - (optional) Exactly two adjacent m's before the y's.
//    - (optional) Separated by white-space and/or a dash or slash.
//  - (optional) Prepended with some text similar to "Expiration Date".
// Tested in components/autofill/core/browser/autofill_regexes_unittest.cc
const char16_t kExpirationDate2DigitYearRe[] =
    u"(?:exp.*date[^y\\n\\r]*|mm\\s*[-/]?\\s*)yy(?:[^y]|$)";
// Used to match a expiration date field with a four digit year.
// Same requirements as |kExpirationDate2DigitYearRe| except:
//  - Exactly four adjacent y's.
// Tested in components/autofill/core/browser/autofill_regexes_unittest.cc
const char16_t kExpirationDate4DigitYearRe[] =
    u"(?:exp.*date[^y\\n\\r]*|mm\\s*[-/]?\\s*)yyyy(?:[^y]|$)";
// Used to match expiration date fields that do not specify a year length.
const char16_t kExpirationDateRe[] =
    u"expir|exp.*date|^expfield$"
    u"|gueltig|gültig"        // de-DE
    u"|fecha"                 // es
    u"|date.*exp"             // fr-FR
    u"|scadenza"              // it-IT
    u"|有効期限"              // ja-JP
    u"|validade"              // pt-BR, pt-PT
    u"|Срок действия карты";  // ru
const char16_t kGiftCardRe[] = u"gift.?(card|cert)";
const char16_t kDebitGiftCardRe[] =
    u"(?:visa|mastercard|discover|amex|american express).*gift.?card";
const char16_t kDebitCardRe[] = u"debit.*card";
const char16_t kDayRe[] = u"day";

/////////////////////////////////////////////////////////////////////////////
// email_field.cc
/////////////////////////////////////////////////////////////////////////////
const char16_t kEmailRe[] =
    u"e.?mail"
    u"|courriel"                     // fr
    u"|correo.*electr(o|ó)nico"      // es-ES
    u"|メールアドレス"               // ja-JP
    u"|Электронн(ая|ой).?Почт(а|ы)"  // ru
    u"|邮件|邮箱|電子郵件"           // zh-CN
    u"|電郵地址|電子信箱"            // zh-TW
    u"|ഇ-മെയില്‍|ഇലക്ട്രോണിക്.?"
    u"മെയിൽ"                                        // ml
    u"|ایمیل|پست.*الکترونیک"                        // fa
    u"|ईमेल|इलॅक्ट्रॉनिक.?मेल"                           // hi
    u"|(\\b|_)eposta(\\b|_)"                        // tr
    u"|(?:이메일|전자.?우편|[Ee]-?mail)(.?주소)?";  // ko-KR

/////////////////////////////////////////////////////////////////////////////
// name_field.cc
/////////////////////////////////////////////////////////////////////////////
const char16_t kNameIgnoredRe[] =
    u"user.?name|user.?id|nickname|maiden name|title|prefix|suffix|mail"
    u"|vollständiger.?name"              // de-DE
    u"|用户名"                           // zh-CN
    u"|(?:사용자.?)?아이디|사용자.?ID";  // ko-KR
const char16_t kFullNameRe[] =
    u"^name|full.?name|your.?name|customer.?name|bill.?name|ship.?name"
    u"|name.*first.*last|firstandlastname|contact.?(name|person)"
    u"|nombre.*y.*apellidos"                    // es
    u"|^nom(?![a-zA-Z])"                        // fr-FR
    u"|お名前|氏名"                             // ja-JP
    u"|^nome"                                   // pt-BR, pt-PT
    u"|نام.*نام.*خانوادگی"                      // fa
    u"|姓\\s*名"                                // zh-CN
    u"|контактное.?лицо"                        // ru
    u"|(\\b|_|\\*)ad[ı]? soyad[ı]?(\\b|_|\\*)"  // tr
    u"|성명"                                    // ko-KR
    u"|nama.?(lengkap|penerima|kamu)";          // id
const char16_t kNameGenericRe[] =
    u"^name"
    u"|^nom"    // fr-FR
    u"|^nome";  // pt-BR, pt-PT
const char16_t kFirstNameRe[] =
    u"first.*name|initials|fname|first$|given.*name"
    u"|vorname"                                             // de-DE
    u"|nombre"                                              // es
    u"|forename|prénom|prenom"                              // fr-FR
    u"|名"                                                  // ja-JP
    u"|nome"                                                // pt-BR, pt-PT
    u"|Имя"                                                 // ru
    u"|نام"                                                 // fa
    u"|이름"                                                // ko-KR
    u"|പേര്"                                                 // ml
    u"|(\\b|_|\\*)(isim|ad|ad(i|ı|iniz|ınız)?)(\\b|_|\\*)"  // tr
    u"|नाम"                                                 // hi
    u"|nama depan";                                         // id
const char16_t kMiddleInitialRe[] = u"middle.*initial|m\\.i\\.|mi$|\\bmi\\b";
const char16_t kMiddleNameRe[] = u"middle.*name|mname|middle$";
const char16_t kLastNameRe[] =
    u"last.*name|lname|surname(?!\\d)|last$|secondname|family.*name"
    u"|nachname"                                               // de-DE
    u"|apellidos?"                                             // es
    u"|famille|^nom(?![a-zA-Z])"                               // fr-FR
    u"|cognome"                                                // it-IT
    u"|姓"                                                     // ja-JP
    u"|apelidos|surename|sobrenome"                            // pt-BR, pt-PT
    u"|Фамилия"                                                // ru
    u"|نام.*خانوادگی"                                          // fa
    u"|उपनाम"                                                  // hi
    u"|മറുപേര്"                                                  // ml
    u"|(\\b|_|\\*)(soyisim|soyad(i|ı|iniz|ınız)?)(\\b|_|\\*)"  // tr
    u"|\\b성(?:[^명]|\\b)"                                     // ko-KR
    u"|nama belakang";                                         // id
const char16_t kNameLastFirstRe[] =
    u"(primer.*apellido)"                 // es
    u"|(apellido1)"                       // es
    u"|(apellido.*paterno)"               // es
    u"|surname_?1|first(\\s|_)?surname";  // es
const char16_t kNameLastSecondRe[] =
    u"(segund.*apellido)"                  // es
    u"|(apellido2)"                        // es
    u"|(apellido.*materno)"                // es
    u"|surname_?2|second(\\s|_)?surname";  // es
const char16_t kHonorificPrefixRe[] =
    u"anrede|titel"                 // de-DE
    u"|tratamiento|encabezamiento"  // es
    u"|^title:?$"  // Matched only if there is no prefix or suffix.
    u"|(salutation(?! and given name))"  // en
    u"|titolo"                           // it-IT
    u"|titre"                            // fr-FR
    u"|обращение|звание"                 // ru
    u"|προσφώνηση"                       // el
    u"|hitap";                           // tr
/////////////////////////////////////////////////////////////////////////////
// phone_field.cc
/////////////////////////////////////////////////////////////////////////////
const char16_t kPhoneRe[] =
    u"phone|mobile|contact.?number"
    u"|telefonnummer"                                   // de-DE
    u"|telefono|teléfono"                               // es
    u"|telfixe"                                         // fr-FR
    u"|電話"                                            // ja-JP
    u"|telefone|telemovel"                              // pt-BR, pt-PT
    u"|телефон"                                         // ru
    u"|मोबाइल"                                          // hi for mobile
    u"|(\\b|_|\\*)telefon(\\b|_|\\*)"                   // tr
    u"|电话"                                            // zh-CN
    u"|മൊബൈല്‍"                           // ml for mobile
    u"|(?:전화|핸드폰|휴대폰|휴대전화)(?:.?번호)?"      // ko-KR
    u"|telepon|ponsel|(nomor|no\\.?).?(hp|handphone)";  // id
const char16_t kAugmentedPhoneCountryCodeRe[] =
    u"^[^0-9+]*(?:\\+|00)\\s*([1-9]\\d{0,3})\\D*$";
const char16_t kCountryCodeRe[] =
    u"country.*code|ccode|_cc|phone.*code|user.*phone.*code";
const char16_t kAreaCodeNotextRe[] = u"^\\($";
const char16_t kAreaCodeRe[] =
    u"area.*code|acode|area"
    u"|지역.?번호";  // ko-KR
const char16_t kPhonePrefixSeparatorRe[] = u"^-$|^\\)$";
const char16_t kPhoneSuffixSeparatorRe[] = u"^-$";
const char16_t kPhonePrefixRe[] =
    u"prefix|exchange"
    u"|preselection"  // fr-FR
    u"|ddd";          // pt-BR, pt-PT
const char16_t kPhoneSuffixRe[] = u"suffix";
const char16_t kPhoneExtensionRe[] =
    u"\\bext|ext\\b|extension"
    u"|ramal";  // pt-BR, pt-PT

/////////////////////////////////////////////////////////////////////////////
// travel_field.cc
/////////////////////////////////////////////////////////////////////////////

const char16_t kPassportRe[] =
    u"document.*number|passport"     // en-US
    u"|passeport"                    // fr-FR
    u"|numero.*documento|pasaporte"  // es-ES
    u"|書類";                        // ja-JP
const char16_t kTravelOriginRe[] =
    u"point.*of.*entry|arrival"                // en-US
    u"|punto.*internaci(o|ó)n|fecha.*llegada"  // es-ES
    u"|入国";                                  // ja-JP
const char16_t kTravelDestinationRe[] =
    u"departure"               // en-US
    u"|fecha.*salida|destino"  // es-ES
    u"|出国";                  // ja-JP
const char16_t kFlightRe[] =
    u"airline|flight"                    // en-US
    u"|aerol(i|í)nea|n(u|ú)mero.*vuelo"  // es-ES
    u"|便名|航空会社";                   // ja-JP

/////////////////////////////////////////////////////////////////////////////
// validation.cc
/////////////////////////////////////////////////////////////////////////////
const char16_t kUPIVirtualPaymentAddressRe[] =
    u"^[\\w.+-_]+@("        // eg user@
    u"\\w+\\.ifsc\\.npci|"  // IFSC code
    u"aadhaar\\.npci|"      // Aadhaar number
    u"mobile\\.npci|"       // Mobile number
    u"rupay\\.npci|"        // RuPay card number
    u"airtel|"  // List of banks https://www.npci.org.in/upi-live-members
    u"airtelpaymentsbank|"
    u"albk|"
    u"allahabadbank|"
    u"allbank|"
    u"andb|"
    u"apb|"
    u"apl|"
    u"axis|"
    u"axisbank|"
    u"axisgo|"
    u"bandhan|"
    u"barodampay|"
    u"birla|"
    u"boi|"
    u"cbin|"
    u"cboi|"
    u"centralbank|"
    u"cmsidfc|"
    u"cnrb|"
    u"csbcash|"
    u"csbpay|"
    u"cub|"
    u"dbs|"
    u"dcb|"
    u"dcbbank|"
    u"denabank|"
    u"dlb|"
    u"eazypay|"
    u"equitas|"
    u"ezeepay|"
    u"fbl|"
    u"federal|"
    u"finobank|"
    u"hdfcbank|"
    u"hsbc|"
    u"icici|"
    u"idbi|"
    u"idbibank|"
    u"idfc|"
    u"idfcbank|"
    u"idfcnetc|"
    u"ikwik|"
    u"imobile|"
    u"indbank|"
    u"indianbank|"
    u"indianbk|"
    u"indus|"
    u"iob|"
    u"jkb|"
    u"jsb|"
    u"jsbp|"
    u"karb|"
    u"karurvysyabank|"
    u"kaypay|"
    u"kbl|"
    u"kbl052|"
    u"kmb|"
    u"kmbl|"
    u"kotak|"
    u"kvb|"
    u"kvbank|"
    u"lime|"
    u"lvb|"
    u"lvbank|"
    u"mahb|"
    u"obc|"
    u"okaxis|"
    u"okbizaxis|"
    u"okhdfcbank|"
    u"okicici|"
    u"oksbi|"
    u"paytm|"
    u"payzapp|"
    u"pingpay|"
    u"pnb|"
    u"pockets|"
    u"psb|"
    u"purz|"
    u"rajgovhdfcbank|"
    u"rbl|"
    u"sbi|"
    u"sc|"
    u"scb|"
    u"scbl|"
    u"scmobile|"
    u"sib|"
    u"srcb|"
    u"synd|"
    u"syndbank|"
    u"syndicate|"
    u"tjsb|"
    u"tjsp|"
    u"ubi|"
    u"uboi|"
    u"uco|"
    u"unionbank|"
    u"unionbankofindia|"
    u"united|"
    u"upi|"
    u"utbi|"
    u"vijayabank|"
    u"vijb|"
    u"vjb|"
    u"ybl|"
    u"yesbank|"
    u"yesbankltd"
    u")$";

const char16_t kInternationalBankAccountNumberRe[] =
    u"^[a-zA-Z]{2}[0-9]{2}[a-zA-Z0-9]{4}[0-9]{7}([a-zA-Z0-9]?){0,16}$";

// Matches all 3 and 4 digit numbers.
const char16_t kCreditCardCVCPattern[] = u"^\\d{3,4}$";

// Matches numbers in the range [2010-2099].
const char16_t kCreditCard4DigitExpYearPattern[] = u"^[2][0][1-9][0-9]$";

/////////////////////////////////////////////////////////////////////////////
// form_structure.cc
/////////////////////////////////////////////////////////////////////////////
const char16_t kUrlSearchActionRe[] = u"/search(/|((\\w*\\.\\w+)?$))";

/////////////////////////////////////////////////////////////////////////////
// form_parser.cc
/////////////////////////////////////////////////////////////////////////////
const char16_t kSocialSecurityRe[] = u"ssn|social.?security.?(num(ber)?|#)*";
const char16_t kOneTimePwdRe[] =
    u"one.?time|sms.?(code|token|password|pwd|pass)";
// Matches strings that consist of one repeated non alphanumeric symbol,
// that is likely a result of website modifying the value to hide it.
const char16_t kHiddenValueRe[] = u"^(\\W)\\1+$";

/////////////////////////////////////////////////////////////////////////////
// merchant_promo_code_field.cc
/////////////////////////////////////////////////////////////////////////////
const char16_t kMerchantPromoCodeRe[] =
    u"\\bpromo.*code\\b|\\bcoupon code\\b|\\bgift code\\b";

/////////////////////////////////////////////////////////////////////////////
// votes_uploader.cc
/////////////////////////////////////////////////////////////////////////////
const char16_t kEmailValueRe[] =
    u"^[a-zA-Z0-9.!#$%&’*+/=?^_`{|}~-]+@[a-zA-Z0-9-]+(?:\\.[a-zA-Z0-9-]+)*$";
const char16_t kPhoneValueRe[] = u"^[0-9()+-]{6,25}$";
const char16_t kUsernameLikeValueRe[] = u"[A-Za-z0-9_\\-.]{7,30}";

}  // namespace autofill
