// ─────────────────────────────────────────────────────────────────────────────
// tinyxml2_doc_impl.cpp — IXmlNode / IXmlDoc 的 tinyxml2 实现
// ─────────────────────────────────────────────────────────────────────────────

#include "tinyxml2_doc_impl.hpp"

#include <sstream>
#include <stdexcept>

namespace corekit {
namespace xml {
namespace detail {

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static const char* SafeStr(const char* s) { return s ? s : ""; }

// ─────────────────────────────────────────────────────────────────────────────
// TinyXmlNodeImpl — 查
// ─────────────────────────────────────────────────────────────────────────────

std::string TinyXmlNodeImpl::Name() const {
  return SafeStr(elem_->Name());
}

std::string TinyXmlNodeImpl::Text() const {
  return SafeStr(elem_->GetText());
}

std::string TinyXmlNodeImpl::Attr(const std::string& name,
                                   const std::string& default_val) const {
  const char* v = elem_->Attribute(name.c_str());
  return v ? std::string(v) : default_val;
}

bool TinyXmlNodeImpl::HasAttr(const std::string& name) const {
  return elem_->Attribute(name.c_str()) != nullptr;
}

std::vector<std::pair<std::string, std::string>>
TinyXmlNodeImpl::Attrs() const {
  std::vector<std::pair<std::string, std::string>> result;
  for (const tinyxml2::XMLAttribute* a = elem_->FirstAttribute();
       a != nullptr; a = a->Next()) {
    result.emplace_back(SafeStr(a->Name()), SafeStr(a->Value()));
  }
  return result;
}

XmlNodePtr TinyXmlNodeImpl::FirstChild(const std::string& tag) const {
  tinyxml2::XMLElement* c =
      tag.empty() ? elem_->FirstChildElement()
                  : elem_->FirstChildElement(tag.c_str());
  return Wrap(c);
}

XmlNodePtr TinyXmlNodeImpl::NextSibling(const std::string& tag) const {
  tinyxml2::XMLElement* s =
      tag.empty() ? elem_->NextSiblingElement()
                  : elem_->NextSiblingElement(tag.c_str());
  return Wrap(s);
}

std::vector<XmlNodePtr> TinyXmlNodeImpl::Children(const std::string& tag) const {
  std::vector<XmlNodePtr> result;
  tinyxml2::XMLElement* c =
      tag.empty() ? elem_->FirstChildElement()
                  : elem_->FirstChildElement(tag.c_str());
  while (c) {
    result.push_back(Wrap(c));
    c = tag.empty() ? c->NextSiblingElement()
                    : c->NextSiblingElement(tag.c_str());
  }
  return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// TinyXmlNodeImpl — 改
// ─────────────────────────────────────────────────────────────────────────────

void TinyXmlNodeImpl::SetText(const std::string& text) {
  elem_->SetText(text.c_str());
}

void TinyXmlNodeImpl::SetAttr(const std::string& name,
                               const std::string& value) {
  elem_->SetAttribute(name.c_str(), value.c_str());
}

void TinyXmlNodeImpl::RemoveAttr(const std::string& name) {
  elem_->DeleteAttribute(name.c_str());
}

// ─────────────────────────────────────────────────────────────────────────────
// TinyXmlNodeImpl — 增
// ─────────────────────────────────────────────────────────────────────────────

XmlNodePtr TinyXmlNodeImpl::AppendChild(const std::string& tag) {
  tinyxml2::XMLElement* child = xml_doc_->NewElement(tag.c_str());
  elem_->InsertEndChild(child);
  return Wrap(child);
}

// ─────────────────────────────────────────────────────────────────────────────
// TinyXmlNodeImpl — 删
// ─────────────────────────────────────────────────────────────────────────────

bool TinyXmlNodeImpl::RemoveFirstChild(const std::string& tag) {
  tinyxml2::XMLElement* c = elem_->FirstChildElement(tag.c_str());
  if (!c) return false;
  elem_->DeleteChild(c);
  return true;
}

void TinyXmlNodeImpl::RemoveAllChildren(const std::string& tag) {
  if (tag.empty()) {
    elem_->DeleteChildren();
    return;
  }
  // Collect then delete to avoid iterator invalidation
  std::vector<tinyxml2::XMLElement*> to_delete;
  for (tinyxml2::XMLElement* c = elem_->FirstChildElement(tag.c_str());
       c != nullptr; c = c->NextSiblingElement(tag.c_str())) {
    to_delete.push_back(c);
  }
  for (auto* c : to_delete) {
    elem_->DeleteChild(c);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// TinyXmlNodeImpl — private helper
// ─────────────────────────────────────────────────────────────────────────────

XmlNodePtr TinyXmlNodeImpl::Wrap(tinyxml2::XMLElement* e) const {
  if (!e) return nullptr;
  return std::make_shared<TinyXmlNodeImpl>(e, xml_doc_);
}

// ─────────────────────────────────────────────────────────────────────────────
// TinyXmlDocImpl — ctor
// ─────────────────────────────────────────────────────────────────────────────

TinyXmlDocImpl::TinyXmlDocImpl()
    : doc_(std::make_shared<tinyxml2::XMLDocument>()) {}

// ─────────────────────────────────────────────────────────────────────────────
// TinyXmlDocImpl — 加载 / 解析
// ─────────────────────────────────────────────────────────────────────────────

api::Status TinyXmlDocImpl::LoadFromFile(const std::string& path) {
  tinyxml2::XMLError err = doc_->LoadFile(path.c_str());
  return TranslateError(err, *doc_);
}

api::Status TinyXmlDocImpl::LoadFromString(const std::string& xml) {
  tinyxml2::XMLError err = doc_->Parse(xml.c_str(), xml.size());
  return TranslateError(err, *doc_);
}

// ─────────────────────────────────────────────────────────────────────────────
// TinyXmlDocImpl — 保存 / 序列化
// ─────────────────────────────────────────────────────────────────────────────

api::Status TinyXmlDocImpl::SaveToFile(const std::string& path) const {
  tinyxml2::XMLError err = doc_->SaveFile(path.c_str());
  if (err == tinyxml2::XML_ERROR_FILE_COULD_NOT_BE_OPENED) {
    return api::Status(api::StatusCode::kIoError,
                       std::string("xml file could not be opened for write: ") + path,
                       api::ErrorModule::kXml);
  }
  return TranslateError(err, *doc_);
}

std::string TinyXmlDocImpl::ToString() const {
  tinyxml2::XMLPrinter printer;
  doc_->Print(&printer);
  return std::string(printer.CStr());
}

// ─────────────────────────────────────────────────────────────────────────────
// TinyXmlDocImpl — 根节点访问
// ─────────────────────────────────────────────────────────────────────────────

XmlNodePtr TinyXmlDocImpl::Root() const {
  return Wrap(doc_->RootElement());
}

XmlNodePtr TinyXmlDocImpl::CreateRoot(const std::string& tag) {
  // Remove existing root if present
  if (doc_->RootElement()) {
    doc_->DeleteChild(doc_->RootElement());
  }
  tinyxml2::XMLElement* root = doc_->NewElement(tag.c_str());
  doc_->InsertEndChild(root);
  return Wrap(root);
}

// ─────────────────────────────────────────────────────────────────────────────
// TinyXmlDocImpl — 路径查询
// ─────────────────────────────────────────────────────────────────────────────

XmlNodePtr TinyXmlDocImpl::Find(const std::string& path) const {
  if (path.empty()) return nullptr;

  // Split by '/'
  std::vector<std::string> parts;
  std::stringstream ss(path);
  std::string seg;
  while (std::getline(ss, seg, '/')) {
    if (!seg.empty()) parts.push_back(seg);
  }
  if (parts.empty()) return nullptr;

  // Start from root
  tinyxml2::XMLElement* cur = doc_->RootElement();
  if (!cur) return nullptr;

  // First part must match root name
  if (std::string(SafeStr(cur->Name())) != parts[0]) return nullptr;

  for (size_t i = 1; i < parts.size(); ++i) {
    cur = cur->FirstChildElement(parts[i].c_str());
    if (!cur) return nullptr;
  }
  return Wrap(cur);
}

// ─────────────────────────────────────────────────────────────────────────────
// TinyXmlDocImpl — private helpers
// ─────────────────────────────────────────────────────────────────────────────

XmlNodePtr TinyXmlDocImpl::Wrap(tinyxml2::XMLElement* e) const {
  if (!e) return nullptr;
  return std::make_shared<TinyXmlNodeImpl>(e, doc_);
}

api::Status TinyXmlDocImpl::TranslateError(tinyxml2::XMLError err,
                                            const tinyxml2::XMLDocument& doc) {
  if (err == tinyxml2::XML_SUCCESS) return api::Status::Ok();

  const char* raw = doc.ErrorStr();
  std::string msg = raw ? std::string(raw) : std::string("xml error ") + std::to_string(err);

  if (err == tinyxml2::XML_ERROR_FILE_NOT_FOUND ||
      err == tinyxml2::XML_ERROR_FILE_COULD_NOT_BE_OPENED ||
      err == tinyxml2::XML_ERROR_FILE_READ_ERROR) {
    return api::Status(api::StatusCode::kNotFound, msg, api::ErrorModule::kXml);
  }
  // tinyxml2 v10 does not expose a separate write-error code;
  // kIoError is returned for SaveFile failures via XML_ERROR_FILE_COULD_NOT_BE_OPENED
  // when opening for write.  Catch the general "could not be opened" case below.
  return api::Status(api::StatusCode::kInvalidArgument, msg, api::ErrorModule::kXml);
}

}  // namespace detail

// ── 工厂函数 ─────────────────────────────────────────────────────────────────

XmlDocPtr CreateXmlDoc() {
  return std::make_shared<detail::TinyXmlDocImpl>();
}

}  // namespace xml
}  // namespace corekit
