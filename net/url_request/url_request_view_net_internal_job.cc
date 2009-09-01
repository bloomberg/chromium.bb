// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "net/url_request/url_request_view_net_internal_job.h"

#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "net/base/escape.h"
#include "net/base/load_log_util.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"

namespace {

//------------------------------------------------------------------------------
// Format helpers.
//------------------------------------------------------------------------------

void OutputTextInPre(const std::string& text, std::string* out) {
  out->append("<pre>");
  out->append(EscapeForHTML(text));
  out->append("</pre>");
}

//------------------------------------------------------------------------------
// Subsection definitions.
//------------------------------------------------------------------------------

class SubSection {
 public:
  // |name| is the URL path component for this subsection.
  // |title| is the textual description.
  SubSection(SubSection* parent, const char* name, const char* title)
      : parent_(parent),
        name_(name),
        title_(title) {
  }

  virtual ~SubSection() {
    STLDeleteContainerPointers(children_.begin(), children_.end());
  }

  // Outputs the subsection's contents to |out|.
  virtual void OutputBody(URLRequestContext* context, std::string* out) {}

  // Outputs this subsection, and all of its children.
  void OutputRecursive(URLRequestContext* context, std::string* out) {
    if (!is_root()) {
      // Print the heading.
      out->append(StringPrintf("<div>"
          "<span class=subsection_title>%s</span> "
          "<span class=subsection_name>(about:net-internal/%s)<span>"
          "</div>",
          EscapeForHTML(title_).c_str(),
          EscapeForHTML(GetFullyQualifiedName()).c_str()));

      out->append("<div class=subsection_body>");
    }

    OutputBody(context, out);

    for (size_t i = 0; i < children_.size(); ++i)
      children_[i]->OutputRecursive(context, out);

    if (!is_root())
      out->append("</div>");
  }

  // Returns the SubSection contained by |this| with fully qualified name
  // |dotted_name|, or NULL if none was found.
  SubSection* FindSubSectionByName(const std::string& dotted_name) {
    if (dotted_name == "")
      return this;

    std::string child_name;
    std::string child_sub_name;

    size_t split_pos = dotted_name.find('.');
    if (split_pos == std::string::npos) {
      child_name = dotted_name;
      child_sub_name = std::string();
    } else {
      child_name = dotted_name.substr(0, split_pos);
      child_sub_name = dotted_name.substr(split_pos + 1);
    }

    for (size_t i = 0; i < children_.size(); ++i) {
      if (children_[i]->name_ == child_name)
        return children_[i]->FindSubSectionByName(child_sub_name);
    }

    return NULL;  // Not found.
  }

  std::string GetFullyQualifiedName() {
    if (!parent_)
      return name_;

    std::string parent_name = parent_->GetFullyQualifiedName();
    if (parent_name.empty())
      return name_;

    return parent_name + "." + name_;
  }

  bool is_root() const {
    return parent_ == NULL;
  }

 protected:
  typedef std::vector<SubSection*> SubSectionList;

  void AddSubSection(SubSection* subsection) {
    children_.push_back(subsection);
  }

  SubSection* parent_;
  std::string name_;
  std::string title_;

  SubSectionList children_;
};

class ProxyServiceCurrentConfigSubSection : public SubSection {
 public:
  ProxyServiceCurrentConfigSubSection(SubSection* parent)
      : SubSection(parent, "config", "Current configuration") {
  }

  virtual void OutputBody(URLRequestContext* context, std::string* out) {
    net::ProxyService* proxy_service = context->proxy_service();
    if (proxy_service->config_has_been_initialized()) {
      // net::ProxyConfig defines an operator<<.
      std::ostringstream stream;
      stream << proxy_service->config();
      OutputTextInPre(stream.str(), out);
    } else {
      out->append("<i>Not yet initialized</i>");
    }
  }
};

class ProxyServiceLastInitLogSubSection : public SubSection {
 public:
  ProxyServiceLastInitLogSubSection(SubSection* parent)
      : SubSection(parent, "init_log", "Last initialized load log") {
  }

  virtual void OutputBody(URLRequestContext* context, std::string* out) {
    net::ProxyService* proxy_service = context->proxy_service();
    net::LoadLog* log = proxy_service->init_proxy_resolver_log();
    if (log) {
      OutputTextInPre(net::LoadLogUtil::PrettyPrintAsEventTree(log), out);
    } else {
      out->append("<i>None.</i>");
    }
  }
};

class ProxyServiceBadProxiesSubSection : public SubSection {
 public:
  ProxyServiceBadProxiesSubSection(SubSection* parent)
      : SubSection(parent, "bad_proxies", "Bad Proxies") {
  }

  virtual void OutputBody(URLRequestContext* context, std::string* out) {
    out->append("TODO");
  }
};

class ProxyServiceSubSection : public SubSection {
 public:
  ProxyServiceSubSection(SubSection* parent)
      : SubSection(parent, "proxyservice", "ProxyService") {
    AddSubSection(new ProxyServiceCurrentConfigSubSection(this));
    AddSubSection(new ProxyServiceLastInitLogSubSection(this));
    AddSubSection(new ProxyServiceBadProxiesSubSection(this));
  }
};

class HostResolverCacheSubSection : public SubSection {
 public:
  HostResolverCacheSubSection(SubSection* parent)
      : SubSection(parent, "hostcache", "HostCache") {
  }

  virtual void OutputBody(URLRequestContext* context, std::string* out) {
    out->append("TODO");
  }
};

class HostResolverSubSection : public SubSection {
 public:
  HostResolverSubSection(SubSection* parent)
      : SubSection(parent, "hostresolver", "HostResolver") {
    AddSubSection(new HostResolverCacheSubSection(this));
  }
};

class URLRequestSubSection : public SubSection {
 public:
  URLRequestSubSection(SubSection* parent)
      : SubSection(parent, "urlrequest", "URLRequest") {
  }

  virtual void OutputBody(URLRequestContext* context, std::string* out) {
    out->append("TODO");
  }
};

class AllSubSections : public SubSection {
 public:
  AllSubSections() : SubSection(NULL, "", "") {
    AddSubSection(new ProxyServiceSubSection(this));
    AddSubSection(new HostResolverSubSection(this));
    AddSubSection(new URLRequestSubSection(this));
  }
};

}  // namespace

// static
URLRequestJob* URLRequestViewNetInternalJob::Factory(
    URLRequest* request, const std::string& scheme) {
  return new URLRequestViewNetInternalJob(request);
}

bool URLRequestViewNetInternalJob::GetData(std::string* mime_type,
                                           std::string* charset,
                                           std::string* data) const {
  DCHECK_EQ(std::string("view-net-internal"), request_->url().scheme());

  mime_type->assign("text/html");
  charset->assign("UTF-8");

  URLRequestContext* context = request_->context();
  std::string path = request_->url().path();

  data->clear();
  data->append("<html><head><title>Network internals</title>"
               "<style>"
               "body { font-family: sans-serif; }\n"
               ".subsection_body { margin: 10px 0 10px 2em; }\n"
               ".subsection_title { font-weight: bold; }\n"
               ".subsection_name { font-size: 80%; }\n"
               "</style>"
               "</head><body>");

  SubSection* all = Singleton<AllSubSections>::get();
  SubSection* section = all;

  // Display only the subsection tree asked for.
  if (!path.empty())
    section = all->FindSubSectionByName(path);

  if (section) {
    section->OutputRecursive(context, data);
  } else {
    data->append("<i>Nothing found for \"");
    data->append(EscapeForHTML(path));
    data->append("\"</i>");
  }

  data->append("</body></html>");

  return true;
}

