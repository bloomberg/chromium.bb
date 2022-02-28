<style>
.note::before {
  content: 'Note: ';
  font-variant: small-caps;
  font-style: italic;
}

.doc h1 {
  margin: 0;
}
</style>

# Creating WebUI Interfaces outside components/
This guide is based on [Creating WebUI Interfaces in components](webui_in_components.md), and comments from reviewers when creating the ChromeOS emoji picker.

[TOC]

<a name="creating_web_ui_page"></a>
WebUI pages live in `chrome/browser/resources`.  You should create a folder for your project `chrome/browser/resources/hello_world`.
When creating WebUI resources, follow the [Web Development Style Guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/web/web.md). For a sample WebUI page you could start with the following files:

`chrome/browser/resources/hello_world/hello_world_container.html`
```html
<!DOCTYPE HTML>
<html>
  <meta charset="utf-8">
  <link rel="stylesheet" href="hello_world.css">
  <hello-world></hello-world>
  <script type="module" src="hello_world.js"></script>
</html>
```

`chrome/browser/resources/hello_world/hello_world.css`
```css
body {
  margin: 0;
}
```

`chrome/browser/resources/hello_world/hello_world.html`
```html
<h1>Hello World</h1>
<div id="example-div">[[message_]]</div>
```

`chrome/browser/resources/hello_world/hello_world.js`
```js
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
export class HelloWorldElement extends PolymerElement {
  static get is() {
    return 'hello-world';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      message_: {
        type: String,
        value: () => loadTimeData.getString('message'),
      },
    };
  }
}

customElements.define(HelloWorldElement.is, HelloWorldElement);
```

Add a `BUILD.gn` file to get Javascript type checking and Polymer compilation:

`chrome/browser/resources/hello_world/BUILD.gn`
```
import("//third_party/closure_compiler/compile_js.gni")
import("//tools/polymer/html_to_js.gni")

html_to_js("web_components") {
  js_files = [ "hello_world.js" ]
}

js_library("hello_world") {
  deps = [
    "//ui/webui/resources/js:load_time_data.m",
    "//ui/webui/resources/js:util.m",
  ]
}

js_type_check("closure_compile") {
  deps = [ ":hello_world" ]
}
```

Add the new `:closure_compile` target to `chrome/browser/resources/BUILD.gn` to
include it in coverage:

```
group("closure_compile) {
  deps = [
    ...
    "hello_world:closure_compile"
    ...
  ]
```

Finally, create an `OWNERS` file for the new folder.

## Adding the resources
Resources for the browser are stored in `grd` files.  Current best practice is to autogenerate a grd file for your
component in the `BUILD` file we created earlier

`chrome/browser/resources/hello_world/BUILD.gn`
```
import("//tools/grit/grit_rule.gni")
import("//tools/grit/preprocess_if_expr.gni")
import("//ui/webui/resources/tools/generate_grd.gni")

preprocess_folder = "preprocessed"
preprocess_gen_manifest = "preprocessed_gen_manifest.json"
resources_grd_file = "$target_gen_dir/resources.grd"

preprocess_if_expr("preprocess_generated") {
  deps = [ ":web_components" ]
  in_folder = target_gen_dir
  out_folder = "$target_gen_dir/$preprocess_folder"
  out_manifest = "$target_gen_dir/$preprocess_gen_manifest"
  in_files = [ "hello_world.js" ]
}

generate_grd("build_grd") {
  grd_prefix = "hello_world"
  out_grd = resources_grd_file
  input_files = [
    "hello_world.css",
    "hello_world_container.html",
  ]
  input_files_base_dir = rebase_path(".", "//")
  deps = [ ":preprocess_generated" ]
  manifest_files = [ "$target_gen_dir/$preprocess_gen_manifest" ]
}

grit("resources") {
  enable_input_discovery_for_gn_analyze = false
  source = resources_grd_file
  deps = [ ":build_grd" ]
  outputs = [
    "grit/hello_world_resources.h",
    "grit/hello_world_resources_map.cc",
    "grit/hello_world_resources_map.h",
    "hello_world_resources.pak",
  ]
  output_dir = "$root_gen_dir/chrome"
}
```

Then add the new resource target to `chrome/browser/resources/BUILD.gn`
```
group("resources") {
  public_deps += [
    ...
    "hello_world:resources"
    ...
  ]
}
```

Also add to `chrome/chrome_paks.gni`

```
template("chrome_extra_paks") {
  ... (lots)
  sources += [
    ...
    "$root_gen_dir/chrome/hello_world_resources.pak",
    ...
  ]
  deps += [
    ...
    "//chrome/browser/resources/hello_world:resources",
    ...
  ]
}
```

## Adding URL constants for the new chrome URL

`chrome/common/webui_url_constants.cc:`
```c++
const char kChromeUIHelloWorldURL[] = "chrome://hello-world/";
const char kChromeUIHelloWorldHost[] = "hello-world";
```

`chrome/common/webui_url_constants.h:`
```c++
extern const char kChromeUIHelloWorldURL[];
extern const char kChromeUIHelloWorldHost[];
```

## Adding a WebUI class for handling requests to the chrome://hello-world/ URL
Next we need a class to handle requests to this new resource URL. Typically this will subclass `WebUIController` (WebUI
dialogs will also need another class which will subclass `WebDialogDelegate`, this is shown later).

`chrome/browser/ui/webui/hello_world_ui.h`
```c++
#ifndef CHROME_BROWSER_UI_WEBUI_HELLO_WORLD_HELLO_WORLD_H_
#define CHROME_BROWSER_UI_WEBUI_HELLO_WORLD_HELLO_WORLD_H_

#include "content/public/browser/web_ui_controller.h"

// The WebUI for chrome://hello-world
class HelloWorldUI : public content::WebUIController {
 public:
  explicit HelloWorldUI(content::WebUI* web_ui);
  ~HelloWorldUI() override;
};

#endif // CHROME_BROWSER_UI_WEBUI_HELLO_WORLD_HELLO_WORLD_H_
```

`chrome/browser/ui/webui/hello_world_ui.cc`
```c++
#include "chrome/browser/ui/webui/hello_world_ui.h"

#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "chrome/grit/hello_world_resources.h"
#include "chrome/grit/hello_world_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"


HelloWorldUI::HelloWorldUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://hello-world source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIHelloWorldHost);

  // As a demonstration of passing a variable for JS to use we pass in some
  // a simple message.
  html_source->AddString("message", "Hello World!");
  html_source->UseStringsJs();

  // Add required resources.
  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kHelloWorldResources, kHelloWorldResourcesSize),
      IDR_HELLO_WORLD_HELLO_WORLD_CONTAINER_HTML);

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, html_source);
}

HelloWorldUI::~HelloWorldUI() = default;
```

To ensure that your code actually gets compiled, you need to add it to `chrome/browser/ui/BUILD.gn`:

```
static_library("ui") {
  sources = [
    ... (lots)
    "webui/hello_world_ui.cc",
    "webui/hello_world_ui.h",
```

## Adding your WebUI request handler to the Chrome WebUI factory

The Chrome WebUI factory is where you setup your new request handler.

`chrome/browser/ui/webui/chrome_web_ui_controller_factory.cc:`
```c++
+ #include "chrome/browser/ui/webui/hello_world_ui.h"
...
+ if (url.host() == chrome::kChromeUIHelloWorldHost)
+   return &NewWebUI<HelloWorldUI>;
```

## Add an entry to resource_ids.spec
This file is for automatically generating resource ids. Ensure that your entry
has a unique ID and preserves numerical ordering.

`tools/gritsettings/resource_ids.spec`

```
  # START chrome/ WebUI resources section
  ... (lots)
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/hello_world/resources.grd": {
    "META": {"sizes": {"includes": [5]}},
    "includes": [2085],
  },
```

## Check everything works

You're done! Assuming no errors (because everyone gets their code perfect the first time) you should be able to compile
and run chrome and navigate to `chrome://hello-world/` and see your nifty welcome text!


## Making a WebUI Dialog

Instead of having a full page for your WebUI, you might want a dialog in order to have a fully independent window.  To
do that, some small changes are needed to your code.  First, we need to add a new class which inherits from
`ui::WebDialogDelegate`.  The easiest way to do that is to edit the `hello_world_ui.*` files


`chrome/browser/ui/webui/hello_world_ui.h`
```c++
 // Leave the old content, but add this new code
 class HelloWorldDialog : public ui::WebDialogDelegate {
 public:
  static void Show();
  ~HelloWorldDialog() override;
  HelloWorldDialog(const HelloWorldDialog&) = delete;
  HelloWorldDialog& operator=(const HelloWorldDialog&) = delete;

 private:
  HelloWorldDialog();
  // ui::WebDialogDelegate:
  ui::ModalType GetDialogModalType() const override;
  std::u16string GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnDialogShown(content::WebUI* webui) override;
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;

  content::WebUI* webui_ = nullptr;
};
```

`chrome/browser/ui/webui/hello_world_ui.cc`
```c++
 // Leave the old content, but add this new stuff

HelloWorldDialog::HelloWorldDialog() = default;

void HelloWorldDialog::Show() {
  // HelloWorldDialog is self-deleting via OnDialogClosed().
  chrome::ShowWebDialog(nullptr, ProfileManager::GetActiveUserProfile(),
                        new HelloWorldDialog());
}

ui::ModalType HelloWorldDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_NONE;
}

std::u16string HelloWorldDialog::GetDialogTitle() const {
  return u"Hello world";
}

GURL HelloWorldDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIHelloWorldURL[);
}

void HelloWorldDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

void HelloWorldDialog::GetDialogSize(gfx::Size* size) const {
  const int kDefaultWidth = 544;
  const int kDefaultHeight = 628;
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

std::string HelloWorldDialog::GetDialogArgs() const {
  return "";
}

void HelloWorldDialog::OnDialogShown(content::WebUI* webui) {
  webui_ = webui;
}

void HelloWorldDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void HelloWorldDialog::OnCloseContents(content::WebContents* source,
                                        bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool HelloWorldDialog::ShouldShowDialogTitle() const {
  return true;
}

HelloWorldDialog::~HelloWorldDialog() = default;
```

Finally, you will need to do something to actually show your dialog, which can be done by calling `HelloWorldDialog::Show()`.
