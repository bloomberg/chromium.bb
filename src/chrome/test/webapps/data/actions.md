# Critical User Journey Action Catalog for the dPWA product

This file catalogs all of the actions that can be used to build critical user journeys for the dPWA product.

Existing documentation lives [here](/docs/webapps/integration-testing-framework.md).

TODO(dmurph): Move more documentation here. https://crbug.com/1314822

## How this file is parsed

The tables in this file are parsed as action templates for critical user journeys. Lines are considered an action template if:
- The first non-whitespace character is a `|`
- Splitting the line using the `|` character as a delimiter, the first item (stripping whitespace):
  - Does not start with `#`
  - Is not `---`
  - Is not empty


## Actions Table

TODO(dmurph): Possibly this table up into markdown-header section.

| # Action base name | Argument Types | Output Actions | Unique Identifier (next: 123) | Status (WIP, Implemented, Not Implemented, Parameterized) | Description | Metadata, implementation bug, etc |
| --- | --- | --- | --- | --- | --- | --- |
| # Badging |
| check_app_badge_empty | Site |  | 2 | Not Implemented | Check that the 'badge' on the app icon is empty |  |
| check_app_badge_has_value | Site |  | 3 | Not Implemented | Check that the 'badge' on the app icon has a value |  |
| clear_app_badge | Site |  | 4 | Not Implemented | The WebApp clears the 'badge' value from it's icon |  |
| set_app_badge | Site |  | 6 | Not Implemented | Set the app badge for the given site to a value. |  |
| # Manifest Update |
| accept_app_id_update_dialog |  |  | 91 | Implemented | Click Accept in the App Identity Update dialog | finnur@ |
| manifest_update_scope_site_a_foo_to | Site |  | 8 | Implemented | Update the scope of site a/foo/, to the given scope |  |
| manifest_update_display_browser | Site |  | 70 | Implemented | Updates the display property of the manifest to 'browser' on the given site's manifest | cliffordcheng@, P1 |
| manifest_update_display_minimal | Site |  | 36 | Implemented | Updates the display property of the manifest to 'minimal' on the given site's manifest |  |
| manifest_update_icon | Site |  | 68 | Implemented | Updates the launcher icon in the manifest of the website. | finnur@ |
| manifest_update_title | Site |  | 88 | Implemented | The website updates it's manifest.json to change the 'title' | finnur@ |
| manifest_update_colors | Site |  | 80 | Not Implemented | The website updates it's manifest.json to change the 'theme' color | P3 |
| manifest_update_display | Site, Display |  | 116 | WIP |  |  |
| await_manifest_update | Site |  | 117 | WIP | Does any actions necessary (like closing browser windows) and blocks the execution of the test until the manifest has been updated for the given site. |  |
| check_update_dialog_not_shown |  |  | 92 | WIP |  | finnur@ |
| deny_app_update_dialog |  |  | 93 | WIP |  | finnue@ |
| # Run on OS Login |
| apply_run_on_os_login_policy_allowed | Site |  | 100 | Implemented | Apply WebAppSettings policy for run_on_os_login to be allowed | phillis@ |
| apply_run_on_os_login_policy_blocked | Site |  | 101 | Implemented | Apply WebAppSettings policy for run_on_os_login to be blocked | phillis@ |
| apply_run_on_os_login_policy_run_windowed | Site |  | 102 | Implemented | Apply WebAppSettings policy for run_on_os_login to be run_windowed | phillis@ |
| disable_run_on_os_login | Site |  | 105 | Implemented | Disable run on os login from app settings page | phillis@ |
| enable_run_on_os_login | Site |  | 104 | Implemented | Enable run on os login from app settings page | phillis@ |
| remove_run_on_os_login_policy | Site |  | 103 | Implemented | Remove  run_on_os_login policy for the app in WebAppSettings policy. | phillis@ |
| # Install |
| drag_url_to_apps_list |  |  | 5 | Not Implemented | Open chrome://apps in a separage window, and drag a url (highlight & drag url from the omnibox) to the chrome://apps page |  |
| install_create_shortcut_tabbed | Site |  | 29 | Implemented | Install the given app using the"Create Shortcut" menu option (3-dot->"More Tools"->"Create Shortcut), and de-selecting the "Open in a window" checkbox. |  |
| install_create_shortcut_windowed | Site |  | 30 | Implemented | Install the given app using the"Create Shortcut" menu option (3-dot->"More Tools"->"Create Shortcut), and selecting the "Open in a window" checkbox. |  |
| install_locally | Site |  | 46 | Implemented | Find the app in the app list (chrome://apps) and install it by right-clicking on the app and selecting the 'install' option. Win/Mac/Linux only. |  |
| install_omnibox_icon | InstallableSite |  | 31 | Implemented |  |  |
| install_policy_app_tabbed_no_shortcut | Site |  | 32 | Implemented | Add a force-installed enterprise policy app to the user profile (must be managed profile). tabbed, no platform shortcut |  |
| install_policy_app_windowed_no_shortcut | Site |  | 33 | Implemented | Add a force-installed enterprise policy app to the user profile (must be managed profile). windowed, no platform shortcut |  |
| install_menu_option | InstallableSite |  | 47 | Implemented |  |  |
| install_policy_app_tabbed_shortcut | Site |  | 48 | Implemented | Add a force-installed enterprise policy app to the user profile (must be managed profile). tabbed, with platform shortcut |  |
| install_policy_app_windowed_shortcut | Site |  | 49 | Implemented | Add a force-installed enterprise policy app to the user profile (must be managed profile). windowed, with platform shortcut |  |
| install | Site | install_create_shortcut_windowed($1) & install_omnibox_icon($1) & install_policy_app_windowed_no_shortcut($1) & install_policy_app_windowed_shortcut($1) & install_menu_option($1) & install_create_shortcut_tabbed($1) & install_policy_app_tabbed_shortcut($1) & install_policy_app_tabbed_no_shortcut($1) | 52 | Parameterized |  |  |
| install_by_user | Site | install_create_shortcut_windowed($1) & install_omnibox_icon($1) & install_menu_option($1) & install_create_shortcut_tabbed($1) | 53 | Parameterized |  |  |
| install_by_user_tabbed | Site | install_create_shortcut_tabbed($1) | 54 | Parameterized | Install the web app configured to open in a tab. This is done by the selecting the Tools->"Create Shortcut..." menu option in the 3-dot menu, and deselecting the "open in a window" checkbox |  |
| install_by_user_windowed | Site | install_create_shortcut_windowed($1) & install_omnibox_icon($1) & install_menu_option($1) | 55 | Parameterized | Install the web app configured to open in a window. This is done in 3 different ways: 1) Clicking on the install icon in the omnibox, 2) Selecting the "Install _" menu option in the 3-dot menu, or 3) Selecting the Tools->"Create Shortcut..." menu option in the 3-dot menu and checking the "Open in a window" checkbox |  |
| install_no_shortcut | Site | install_policy_app_windowed_no_shortcut($1) & install_policy_app_tabbed_no_shortcut($1) | 56 | Parameterized |  |  |
| install_policy_app | Site | install_policy_app_windowed_no_shortcut($1) & install_policy_app_tabbed_no_shortcut($1) & install_policy_app_windowed_shortcut($1) & install_policy_app_tabbed_shortcut($1) | 57 | Parameterized |  |  |
| install_policy_app_no_shortcut | Site | install_policy_app_tabbed_no_shortcut($1) & install_policy_app_windowed_no_shortcut($1) | 58 | Parameterized |  |  |
| install_policy_app_tabbed | Site | install_policy_app_tabbed_shortcut($1) & install_policy_app_tabbed_no_shortcut($1) | 59 | Parameterized |  |  |
| install_policy_app_windowed | Site | install_policy_app_windowed_no_shortcut($1) & install_policy_app_windowed_shortcut($1) | 60 | Parameterized |  |  |
| install_tabbed | Site | install_create_shortcut_tabbed($1) & install_policy_app_tabbed_shortcut($1) & install_policy_app_tabbed_no_shortcut($1) | 61 | Parameterized | All installation methods that result in a tabbed webapp. |  |
| install_windowed | Site | install_create_shortcut_windowed($1) & install_omnibox_icon($1) & install_policy_app_windowed_no_shortcut($1) & install_policy_app_windowed_shortcut($1) & install_menu_option($1) | 62 | Parameterized | All installation methods that result in a windowed webapp. |  |
| install_with_shortcut | Site | install_policy_app_windowed_shortcut($1) & install_policy_app_tabbed_shortcut($1) & install_create_shortcut_windowed($1) & install_omnibox_icon($1) & install_menu_option($1) & install_create_shortcut_tabbed($1) | 63 | Parameterized |  |  |
| # Uninstall |
| uninstall_from_os | Site |  | 87 | WIP | Uninstalls the app from OS integration - e.g. Windows Control Panel / Start menu |  |
| uninstall_from_app_settings | Site |  | 98 | Implemented | uninstall an app from app settings page, the app has to be locally installed. | phillis@ |
| uninstall_from_list | Site |  | 10 | Implemented | Uninstall the webapp from wherever apps are listed by chrome. On WML, this is from chrome://apps, and on ChromeOS, this is from the 'launcher' |  |
| uninstall_from_menu | Site |  | 43 | Implemented | Uninstall the webapp from the 3-dot menu in the webapp window |  |
| uninstall_policy_app | Site |  | 44 | Implemented | Remove a force-installed policy app to the user profile (must be managed profile) |  |
| uninstall_by_user | Site | uninstall_from_list($1) & uninstall_from_menu($1) & uninstall_from_os($1) & uninstall_from_app_settings($1) | 67 | Parameterized |  |  |
| uninstall_not_locally_installed | Site | uninstall_from_list($1) & uninstall_from_menu($1) & uninstall_from_os($1) | 99 | Parameterized | Uninstall an app by user, the app can be not locally installed. |  |
| # Checking app state |
| check_app_icon_site_a_is | Color |  | 110 | Implemented | Check that the app icon color is correct | finnur@ |
| check_app_in_list_tabbed | Site |  | 11 | Implemented | Find the app in the app list (on desktop, this is chrome://apps, and on ChromeOS, this is the app drawer). Check that the app opens in a window by right clicking on it to see if the "open in window" option is checked, and by launching it to see if it opens in a separate window. |  |
| check_app_in_list_windowed | Site |  | 12 | Implemented | Find the app in the app list (on desktop, this is chrome://apps, and on ChromeOS, this is the app drawer). Check that the app opens in a tab by right clicking on it to see if the "open in window" option is unchecked, and by launching it to see if it opens in a browser tab (and not a window). |  |
| check_app_list_empty |  |  | 13 | Implemented | The app list is empty (on desktop, this is chrome://apps, and on ChromeOS, this is the app drawer). |  |
| check_app_navigation_is_start_url |  |  | 14 | Implemented |  |  |
| check_app_in_list_not_locally_installed | Site |  | 45 | Implemented | Find the app in the app list (chrome://apps) and check that the given app is in the app list and is not installed. This means the icon is grey, and right clicking on it provides an 'install' option. Win/Mac/Linux only. |  |
| check_app_not_in_list | Site |  | 15 | Implemented | Check that the given app is NOT in the app list. On desktop, this is chrome://apps, and on ChromeOS, this is the app drawer. |  |
| check_app_title_site_a_is | Title |  | 79 | Implemented | Check that the app title is correct | finnur@ |
| check_app_in_list_icon_correct | Site |  | 75 | Not Implemented | Find the app in the app list (on desktop, this is chrome://apps, and on ChromeOS, this is the app drawer). Check that the icon for the given app in the app list is correct. | P2 (dmurph modified - should be easy to fetch icon using web request for chrome://app-icon/<app-id>/<dip> I believe) |
| check_theme_color | Site |  | 76 | Not Implemented | Asserts that the theme color of the given app window is correct. | P3 |
| # Misc UX |
| check_browser_navigation_is_app_settings | Site |  | 109 | Implemented | Check the current browser navigation is chrome://app-settings/<app-id> | phillis@ |
| check_create_shortcut_not_shown |  |  | 85 | WIP | Check that the "Create Shortcut" menu option (3-dot->"More Tools"->"Create Shortcut) is greyed out |  |
| check_create_shortcut_shown |  |  | 86 | WIP | Check that the "Create Shortcut" menu option (3-dot->"More Tools"->"Create Shortcut) is shown |  |
| check_custom_toolbar |  |  | 16 | Implemented | Check that the PWA window has a custom toolbar to show the out-of-scope url. |  |
| check_platform_shortcut_not_exists | Site |  | 84 | WIP | The desktop platform shortcut has been removed. | cliffordcheng@, doc |
| check_install_icon_not_shown |  |  | 17 | Implemented | Check that the "Install" icon in the omnibox is not shown |  |
| check_install_icon_shown |  |  | 18 | Implemented | Check that the "Install" icon in the omnibox is shown |  |
| check_launch_icon_not_shown |  |  | 19 | Implemented |  |  |
| check_launch_icon_shown |  |  | 20 | Implemented |  |  |
| check_no_toolbar |  |  | 21 | Implemented |  |  |
| check_platform_shortcut_and_icon | Site |  | 7 | Implemented | The icon of the platform shortcut (on the desktop) is correct | cliffordcheng@, doc |
| check_run_on_os_login_disabled | Site |  | 107 | Implemented | Check run on os login is disabled. | phillis@ |
| check_run_on_os_login_enabled | Site |  | 106 | Implemented | Check run on os login is enabled. | phillis@ |
| check_tab_created |  |  | 22 | Implemented | A tab was created in a chrome browser window |  |
| check_tab_not_created |  |  | 94 | Implemented | A tab was not created by the last state change action | cliffordcheng@, P1 |
| check_user_cannot_set_run_on_os_login | Site |  | 111 | Implemented | Check user can't change the app's  run_on_os_login state.  |  |
| check_window_closed |  |  | 23 | Implemented | The window was closed |  |
| check_window_created |  |  | 24 | Implemented | A window was created. |  |
| check_window_display_minimal |  |  | 25 | Implemented | Check that the window is a PWA window, and has minimal browser controls. |  |
| check_window_display_standalone |  |  | 26 | Implemented | Check that the window is a PWA window, and has no browser controls. |  |
| close_custom_toolbar |  |  | 27 | Implemented | Press the 'x' button on the custom toolbar that is towards the top of the WebApp window. |  |
| close_pwa |  |  | 28 | Implemented | Close the WebApp window. |  |
| open_app_settings | Site | open_app_settings_from_chrome_apps($1) & open_app_settings_from_app_menu($1) | 95 | Parameterized | Launch chrome://app-settings/<app-id> page | phillis@ |
| open_app_settings_from_app_menu | Site |  | 97 | Implemented |  | phillis@ |
| open_app_settings_from_chrome_apps | Site |  | 96 | Implemented |  | phillis@ |
| open_in_chrome |  |  | 71 | Implemented | Click on the 'open in chrome' link in the 3-dot menu of the app window | cliffordcheng@, P1 |
| set_open_in_tab | Site |  | 50 | Implemented | Uncheck the "open in window" checkbox in the right-click menu of the app icon, in the app list page |  |
| set_open_in_window | Site |  | 51 | Implemented | Check the "open in window" checkbox in the right-click menu of the app icon, in the app list page |  |
| check_window_color_correct | Site |  | 77 | Not Implemented | The color of the window is correct. | P3 |
| check_window_icon_correct |  |  | 78 | Not Implemented |  | P3 |
| create_shortcuts | Site |  | 72 | Not Implemented | "create shortcuts" in chrome://apps | P2 |
| delete_platform_shortcut | Site |  | 74 | Not Implemented | Delete the shortcut that lives on the operating system | P2 |
| delete_profile |  |  | 83 | Not Implemented | Delete the user profile. | P4 |
| # Launching |
| launch_from_chrome_apps | Site |  | 34 | Implemented | Launch the web app by navigating to chrome://apps, and then clicking on the app icon. |  |
| launch_from_launch_icon | Site |  | 35 | Implemented | Launch the web app by navigating the browser to the web app, and selecting the launch icon in the omnibox (intent picker), |  |
| launch_from_menu_option | Site |  | 69 | Implemented | Launch the web app by navigating the browser to the web app, and selecting the "Launch _" menu option in the 3-dot menu. | cliffordcheng@, P1 |
| launch_from_platform_shortcut | Site |  | 1 | Implemented | Launch an app from a platform shortcut on the user's desktop or start menu. | cliffordcheng@, P0 |
| launch | Site | launch_from_menu_option($1) & launch_from_launch_icon($1) & launch_from_chrome_apps($1) & launch_from_platform_shortcut($1) | 64 | Parameterized |  |  |
| launch_from_browser | Site | launch_from_menu_option($1) & launch_from_launch_icon($1) & launch_from_chrome_apps($1) | 65 | Parameterized |  |  |
| launch_from_shortcut_or_list | Site | launch_from_chrome_apps($1) & launch_from_platform_shortcut($1) | 66 | Parameterized | All ways to launch an app that are still available for 'browser' apps. |  |
| # Navigation |
| navigate_browser | Site |  | 37 | Implemented | Navigate the browser to one of the static sites provided by the testing framework. |  |
| navigate_notfound_url |  |  | 38 | Implemented | Navigate to a url that returns a 404 server error. |  |
| navigate_pwa_site_a_to | Site |  | 39 | Implemented | Navigates the PWA window of the SiteA pwa to a given site. |  |
| navigate_pwa_site_a_foo_to | Site |  | 9 | Implemented | Navigates the PWA window of the SiteAFoo pwa to a given site. |  |
| navigate_crashed_url |  |  | 81 | Not Implemented | Navigate to a page that crashes, or simulates a crashed renderer. chrome://crash | P3 |
| navigate_link_target_blank |  |  | 82 | Not Implemented | Click on a href link on the current page, where the target of the link is "_blank" | P3 |
| # Cross-device syncing |
| switch_profile_clients | ProfileClient |  | 40 | Implemented | Switch to a different instance of chrome signed in to the same profile |  |
| sync_turn_off |  |  | 41 | Implemented | Turn chrome sync off for "Apps": chrome://settings/syncSetup/advanced |  |
| sync_turn_on |  |  | 42 | Implemented | Turn chrome sync on for "Apps": chrome://settings/syncSetup/advanced |  |
| switch_incognito_profile |  |  | 73 | Not Implemented | Switch to using incognito mode | P2 |
| check_site_handles_file | Site, FileExtension |  | 118 | Not Implemented |  |  |
| # File handling |
| check_file_handling_dialog | IsShown |  | 119 | Not Implemented |  |  |
| launch_file | FilesOptions |  | 120 | Not Implemented |  |  |
| accept_file_handling_dialog |  |  | 121 | Not Implemented |  |  |
| deny_file_handling_dialog |  |  | 122 | Not Implemented |  |  |
| # Window Controls Overlay
| check_window_controls_overlay_toggle | Site, IsShown |  | 112 | WIP |  |  |
| check_window_controls_overlay | Site, IsOn |  | 113 | WIP |  |  |
| enable_window_controls_overlay | Site |  | 114 | WIP |  |  |
| disable_window_controls_overlay | Site |  | 115 | WIP |  |  |
