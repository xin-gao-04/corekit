#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// corekit/xml/i_xml_doc.hpp
//
// XML 文档读写接口：IXmlNode / IXmlDoc
//
// 设计原则：
//   - 调用方只依赖纯虚接口，不暴露 tinyxml2 类型
//   - IXmlNode 提供增删改查四类操作
//   - IXmlDoc 提供加载/保存/根节点访问/简单路径查询
//   - 节点通过 shared_ptr 持有，文档销毁前节点始终有效
// ─────────────────────────────────────────────────────────────────────────────

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "corekit/api/export.hpp"
#include "corekit/api/status.hpp"

namespace corekit {
namespace xml {

class IXmlNode;
class IXmlDoc;

using XmlNodePtr = std::shared_ptr<IXmlNode>;
using XmlDocPtr  = std::shared_ptr<IXmlDoc>;

// ─────────────────────────────────────────────────────────────────────────────
// IXmlNode — 封装单个 XML 元素节点
//
// 查（Read）：Name / Text / Attr / HasAttr / Attrs
//             FirstChild / NextSibling / Children
// 改（Update）：SetText / SetAttr / RemoveAttr
// 增（Create）：AppendChild
// 删（Delete）：RemoveFirstChild / RemoveAllChildren
// ─────────────────────────────────────────────────────────────────────────────
class COREKIT_API IXmlNode {
 public:
  virtual ~IXmlNode() = default;

  // ── 查（Read） ─────────────────────────────────────────────────────────────

  // 节点标签名（tag name）
  virtual std::string Name() const = 0;

  // 内联文本内容，如 <tag>hello</tag> 返回 "hello"；无文本时返回空串
  virtual std::string Text() const = 0;

  // 返回属性值；属性不存在时返回 default_val
  virtual std::string Attr(const std::string& name,
                            const std::string& default_val = "") const = 0;

  virtual bool HasAttr(const std::string& name) const = 0;

  // 返回全部属性的键值对列表（保持文档顺序）
  virtual std::vector<std::pair<std::string, std::string>> Attrs() const = 0;

  // 返回第一个子节点；tag!="" 时返回首个与 tag 匹配的子节点
  // 不存在时返回 nullptr
  virtual XmlNodePtr FirstChild(const std::string& tag = "") const = 0;

  // 在同级节点中向后查找；tag="" 时返回直接下一兄弟；不存在返回 nullptr
  virtual XmlNodePtr NextSibling(const std::string& tag = "") const = 0;

  // 返回所有子节点；tag!="" 时仅返回同名节点
  virtual std::vector<XmlNodePtr> Children(const std::string& tag = "") const = 0;

  // ── 改（Update） ───────────────────────────────────────────────────────────

  // 设置或替换内联文本内容
  virtual void SetText(const std::string& text) = 0;

  // 设置（或新增）属性 name=value
  virtual void SetAttr(const std::string& name, const std::string& value) = 0;

  // 删除属性；属性不存在时为空操作
  virtual void RemoveAttr(const std::string& name) = 0;

  // ── 增（Create） ───────────────────────────────────────────────────────────

  // 追加一个新子节点，返回指向该节点的共享指针
  virtual XmlNodePtr AppendChild(const std::string& tag) = 0;

  // ── 删（Delete） ───────────────────────────────────────────────────────────

  // 删除第一个与 tag 匹配的子节点；返回 true 表示找到并删除
  virtual bool RemoveFirstChild(const std::string& tag) = 0;

  // 删除所有子节点；tag!="" 时仅删除同名的子节点
  virtual void RemoveAllChildren(const std::string& tag = "") = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// IXmlDoc — 表示一份完整的 XML 文档
// ─────────────────────────────────────────────────────────────────────────────
class COREKIT_API IXmlDoc {
 public:
  virtual ~IXmlDoc() = default;

  // ── 加载 / 解析 ────────────────────────────────────────────────────────────

  // 从文件加载；文件不存在返回 kNotFound，格式错误返回 kInvalidArgument
  virtual api::Status LoadFromFile(const std::string& path) = 0;

  // 从 XML 文本加载；格式错误返回 kInvalidArgument
  virtual api::Status LoadFromString(const std::string& xml) = 0;

  // ── 保存 / 序列化 ──────────────────────────────────────────────────────────

  // 序列化到文件；写入失败返回 kIoError
  virtual api::Status SaveToFile(const std::string& path) const = 0;

  // 序列化为 UTF-8 字符串（带 XML 声明）
  virtual std::string ToString() const = 0;

  // ── 根节点访问 ─────────────────────────────────────────────────────────────

  // 文档为空时返回 nullptr
  virtual XmlNodePtr Root() const = 0;

  // 创建（或替换）根节点，返回指向新根节点的指针（始终不为 nullptr）
  virtual XmlNodePtr CreateRoot(const std::string& tag) = 0;

  // ── 简单路径查询 ───────────────────────────────────────────────────────────

  // path 格式："root/child/grandchild"，按 "/" 逐级匹配 FirstChild
  // 节点不存在时返回 nullptr
  virtual XmlNodePtr Find(const std::string& path) const = 0;
};

// ── 工厂函数 ───────────────────────────────────────────────────────────────

// 创建空的 XML 文档实例（tinyxml2 后端）
COREKIT_API XmlDocPtr CreateXmlDoc();

}  // namespace xml
}  // namespace corekit
