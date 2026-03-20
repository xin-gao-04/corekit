#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// tinyxml2_doc_impl.hpp — tinyxml2 后端私有实现
//
// TinyXmlNodeImpl 持有 shared_ptr<tinyxml2::XMLDocument>，确保即使 IXmlDoc
// 的 shared_ptr 已释放，从 IXmlNode 中继续访问也不会产生悬挂指针。
// ─────────────────────────────────────────────────────────────────────────────

#include "corekit/xml/i_xml_doc.hpp"

// tinyxml2.h 由 CMake 通过 target_include_directories PRIVATE 引入
#include "tinyxml2.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace corekit {
namespace xml {
namespace detail {

// ── TinyXmlNodeImpl ───────────────────────────────────────────────────────────

class TinyXmlNodeImpl : public IXmlNode {
 public:
  // xml_doc 是 back-reference，用于保持底层 XMLDocument 存活
  TinyXmlNodeImpl(tinyxml2::XMLElement*                 elem,
                  std::shared_ptr<tinyxml2::XMLDocument> xml_doc)
      : elem_(elem), xml_doc_(std::move(xml_doc)) {}

  // ── 查 ────────────────────────────────────────────────────────────────────
  std::string Name() const override;
  std::string Text() const override;
  std::string Attr(const std::string& name,
                   const std::string& default_val) const override;
  bool HasAttr(const std::string& name) const override;
  std::vector<std::pair<std::string, std::string>> Attrs() const override;
  XmlNodePtr FirstChild(const std::string& tag)  const override;
  XmlNodePtr NextSibling(const std::string& tag) const override;
  std::vector<XmlNodePtr> Children(const std::string& tag) const override;

  // ── 改 ────────────────────────────────────────────────────────────────────
  void SetText(const std::string& text) override;
  void SetAttr(const std::string& name, const std::string& value) override;
  void RemoveAttr(const std::string& name) override;

  // ── 增 ────────────────────────────────────────────────────────────────────
  XmlNodePtr AppendChild(const std::string& tag) override;

  // ── 删 ────────────────────────────────────────────────────────────────────
  bool RemoveFirstChild(const std::string& tag) override;
  void RemoveAllChildren(const std::string& tag) override;

  tinyxml2::XMLElement* RawElem() const { return elem_; }

 private:
  XmlNodePtr Wrap(tinyxml2::XMLElement* e) const;

  tinyxml2::XMLElement*                 elem_;     // non-owning
  std::shared_ptr<tinyxml2::XMLDocument> xml_doc_; // keep doc alive
};

// ── TinyXmlDocImpl ────────────────────────────────────────────────────────────

class TinyXmlDocImpl : public IXmlDoc {
 public:
  TinyXmlDocImpl();

  api::Status LoadFromFile(const std::string& path) override;
  api::Status LoadFromString(const std::string& xml) override;
  api::Status SaveToFile(const std::string& path) const override;
  std::string ToString() const override;

  XmlNodePtr Root() const override;
  XmlNodePtr CreateRoot(const std::string& tag) override;
  XmlNodePtr Find(const std::string& path) const override;

 private:
  XmlNodePtr Wrap(tinyxml2::XMLElement* e) const;
  static api::Status TranslateError(tinyxml2::XMLError err,
                                    const tinyxml2::XMLDocument& doc);

  // Shared so that TinyXmlNodeImpl instances can keep it alive
  std::shared_ptr<tinyxml2::XMLDocument> doc_;
};

}  // namespace detail
}  // namespace xml
}  // namespace corekit
