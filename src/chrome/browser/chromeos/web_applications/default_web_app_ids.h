// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_DEFAULT_WEB_APP_IDS_H_
#define CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_DEFAULT_WEB_APP_IDS_H_

namespace chromeos {
namespace default_web_apps {

// The URLs used to generate the app IDs MUST match the start_url field of the
// manifest served by the PWA.

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "chrome://camera-app/views/main.html"))
constexpr char kCameraAppId[] = "njfbnohfdkmbmnjapinfcopialeghnmh";

// Generated as: // web_app::GenerateAppIdFromURL(GURL(
//     "https://canvas.apps.chrome/"))
constexpr char kCanvasAppId[] = "ieailfmhaghpphfffooibmlghaeopach";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://mail.google.com/?usp=installed_webapp"))
constexpr char kGmailAppId[] = "fpgpbaljnbnpjbpnlnjjhfeigfpemdda";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://calendar.google.com/calendar/?usp=installed_webapp"))
constexpr char kGoogleCalendarAppId[] = "anejmkjdeaeoepnomjgklommefloacja";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://docs.google.com/document/?usp=installed_webapp"))
constexpr char kGoogleDocsAppId[] = "mpnpojknpmmopombnjdcgaaiekajbnjb";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://drive.google.com/?usp=installed_webapp&lfhs=2"))
constexpr char kGoogleDriveAppId[] = "igddggibmpopknllnjfdleakbkpgoobf";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://www.google.com/maps?force=tt&source=ttpwa"))
constexpr char kGoogleMapsAppId[] = "mnhkaebcjjhencmpkapnbdaogjamfbcj";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://news.google.com/?lfhs=2"))
constexpr char kGoogleNewsAppId[] = "kfgapjallbhpciobgmlhlhokknljkgho";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://docs.google.com/spreadsheets/?usp=installed_webapp"))
constexpr char kGoogleSheetsAppId[] = "fhihpiojkbmbpdjeoajapmgkhlnakfjf";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://docs.google.com/presentation/?usp=installed_webapp"))
constexpr char kGoogleSlidesAppId[] = "kefjledonklijopmnomlcbpllchaibag";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "chrome://help-app/"))
constexpr char kHelpAppId[] = "nbljnnecbjbmifnoehiemkgefbnpoeak";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "chrome://media-app/"))
constexpr char kMediaAppId[] = "jhdjimmaggjajfjphpljagpgkidjilnj";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "chrome://test-system-app/pwa.html"))
constexpr char kMockSystemAppId[] = "maphiehpiinjgiaepbljmopkodkadcbh";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "chrome://os-settings/"))
constexpr char kOsSettingsAppId[] = "odknhmnlageboeamepcngndbggdpaobj";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://books.google.com/ebooks/app"))
constexpr char kPlayBooksAppId[] = "jglfhlbohpgcbefmhdmpancnijacbbji";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://google.com/chromebook/whatsnew/embedded/"))
constexpr char kReleaseNotesAppId[] = "lddhblppcjmenljhdleiahjighahdcje";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "chrome://settings/"))
constexpr char kSettingsAppId[] = "inogagmajamaleonmanpkpkkigmklfad";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://www.showtime.com/"))
constexpr char kShowtimeAppId[] = "eoccpgmpiempcflglfokeengliildkag";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://stadia.google.com/?lfhs=2"))
constexpr char kStadiaAppId[] = "pnkcfpnngfokcnnijgkllghjlhkailce";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://www.youtube.com/?feature=ytca"))
constexpr char kYoutubeAppId[] = "agimnkijcaahngcdmfeangaknmldooml";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://music.youtube.com/?source=pwa"))
constexpr char kYoutubeMusicAppId[] = "cinhimbnkkaeohfgghhklpknlkffjgod";

// Generated as: web_app::GenerateAppIdFromURL(GURL(
//     "https://tv.youtube.com/"))
constexpr char kYoutubeTVAppId[] = "kiemjbkkegajmpbobdfngbmjccjhnofh";

}  // namespace default_web_apps
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_DEFAULT_WEB_APP_IDS_H_
