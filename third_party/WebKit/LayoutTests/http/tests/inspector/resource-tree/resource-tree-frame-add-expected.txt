CONSOLE MESSAGE: line 4: iframe loaded
Tests resource tree model on iframe addition, compares resource tree against golden. Every line is important.


Before addition
====================================
Resources:
script http://127.0.0.1:8000/inspector/inspector-test.js
document http://127.0.0.1:8000/inspector/resource-tree/resource-tree-frame-add.html
script http://127.0.0.1:8000/inspector/resource-tree/resource-tree-test.js
stylesheet http://127.0.0.1:8000/inspector/resource-tree/resources/styles-initial.css
script http://127.0.0.1:8000/inspector/resources-test.js

Resources URL Map:
http://127.0.0.1:8000/inspector/inspector-test.js == http://127.0.0.1:8000/inspector/inspector-test.js
http://127.0.0.1:8000/inspector/resource-tree/resource-tree-frame-add.html == http://127.0.0.1:8000/inspector/resource-tree/resource-tree-frame-add.html
http://127.0.0.1:8000/inspector/resource-tree/resource-tree-test.js == http://127.0.0.1:8000/inspector/resource-tree/resource-tree-test.js
http://127.0.0.1:8000/inspector/resource-tree/resources/styles-initial.css == http://127.0.0.1:8000/inspector/resource-tree/resources/styles-initial.css
http://127.0.0.1:8000/inspector/resources-test.js == http://127.0.0.1:8000/inspector/resources-test.js

Resources Tree:
Frames
    top
        Scripts
            inspector-test.js
            resource-tree-test.js
            resources-test.js
        Stylesheets
            styles-initial.css
        resource-tree-frame-add.html
Sources:
-------- Setting mode: [frame]
top
  resource-tree-frame-add.html
  inspector-test.js
  resource-tree-test.js
  resources-test.js
  styles-initial.css
Sources:
-------- Setting mode: [frame/domain]
top
  127.0.0.1:8000
    resource-tree-frame-add.html
    inspector-test.js
    resource-tree-test.js
    resources-test.js
    styles-initial.css
Sources:
-------- Setting mode: [frame/domain/folder]
top
  127.0.0.1:8000
    inspector
      resource-tree
        resources
          styles-initial.css
        resource-tree-frame-add.html
        resource-tree-test.js
      inspector-test.js
      resources-test.js
Sources:
-------- Setting mode: [domain]
127.0.0.1:8000
  resource-tree-frame-add.html
  inspector-test.js
  resource-tree-test.js
  resources-test.js
  styles-initial.css
Sources:
-------- Setting mode: [domain/folder]
127.0.0.1:8000
  inspector
    resource-tree
      resources
        styles-initial.css
      resource-tree-frame-add.html
      resource-tree-test.js
    inspector-test.js
    resources-test.js

After addition
====================================
Resources:
script http://127.0.0.1:8000/inspector/inspector-test.js
document http://127.0.0.1:8000/inspector/resource-tree/resource-tree-frame-add.html
script http://127.0.0.1:8000/inspector/resource-tree/resource-tree-test.js
document http://127.0.0.1:8000/inspector/resource-tree/resources/resource-tree-frame-add-iframe.html
script http://127.0.0.1:8000/inspector/resource-tree/resources/script-navigated.js
stylesheet http://127.0.0.1:8000/inspector/resource-tree/resources/styles-initial.css
stylesheet http://127.0.0.1:8000/inspector/resource-tree/resources/styles-navigated.css
script http://127.0.0.1:8000/inspector/resources-test.js

Resources URL Map:
http://127.0.0.1:8000/inspector/inspector-test.js == http://127.0.0.1:8000/inspector/inspector-test.js
http://127.0.0.1:8000/inspector/resource-tree/resource-tree-frame-add.html == http://127.0.0.1:8000/inspector/resource-tree/resource-tree-frame-add.html
http://127.0.0.1:8000/inspector/resource-tree/resource-tree-test.js == http://127.0.0.1:8000/inspector/resource-tree/resource-tree-test.js
http://127.0.0.1:8000/inspector/resource-tree/resources/resource-tree-frame-add-iframe.html == http://127.0.0.1:8000/inspector/resource-tree/resources/resource-tree-frame-add-iframe.html
http://127.0.0.1:8000/inspector/resource-tree/resources/script-navigated.js == http://127.0.0.1:8000/inspector/resource-tree/resources/script-navigated.js
http://127.0.0.1:8000/inspector/resource-tree/resources/styles-initial.css == http://127.0.0.1:8000/inspector/resource-tree/resources/styles-initial.css
http://127.0.0.1:8000/inspector/resource-tree/resources/styles-navigated.css == http://127.0.0.1:8000/inspector/resource-tree/resources/styles-navigated.css
http://127.0.0.1:8000/inspector/resources-test.js == http://127.0.0.1:8000/inspector/resources-test.js

Resources Tree:
Frames
    top
        resource-tree-frame-add-iframe.html
            Scripts
                script-navigated.js
            Stylesheets
                styles-navigated.css
            resource-tree-frame-add-iframe.html
        Scripts
            inspector-test.js
            resource-tree-test.js
            resources-test.js
        Stylesheets
            styles-initial.css
        resource-tree-frame-add.html
Sources:
-------- Setting mode: [frame]
top
  resource-tree-frame-add.html
  inspector-test.js
  resource-tree-test.js
  resources-test.js
  styles-initial.css
  resource-tree-frame-add-iframe.html
    resource-tree-frame-add-iframe.html
    script-navigated.js
    styles-navigated.css
Sources:
-------- Setting mode: [frame/domain]
top
  127.0.0.1:8000
    resource-tree-frame-add.html
    inspector-test.js
    resource-tree-test.js
    resources-test.js
    styles-initial.css
  resource-tree-frame-add-iframe.html
    127.0.0.1:8000
      resource-tree-frame-add-iframe.html
      script-navigated.js
      styles-navigated.css
Sources:
-------- Setting mode: [frame/domain/folder]
top
  127.0.0.1:8000
    inspector
      resource-tree
        resources
          styles-initial.css
        resource-tree-frame-add.html
        resource-tree-test.js
      inspector-test.js
      resources-test.js
  resource-tree-frame-add-iframe.html
    127.0.0.1:8000
      inspector/resource-tree/resources
        resource-tree-frame-add-iframe.html
        script-navigated.js
        styles-navigated.css
Sources:
-------- Setting mode: [domain]
127.0.0.1:8000
  resource-tree-frame-add-iframe.html
  resource-tree-frame-add.html
  inspector-test.js
  resource-tree-test.js
  resources-test.js
  script-navigated.js
  styles-initial.css
  styles-navigated.css
Sources:
-------- Setting mode: [domain/folder]
127.0.0.1:8000
  inspector
    resource-tree
      resources
        resource-tree-frame-add-iframe.html
        script-navigated.js
        styles-initial.css
        styles-navigated.css
      resource-tree-frame-add.html
      resource-tree-test.js
    inspector-test.js
    resources-test.js

