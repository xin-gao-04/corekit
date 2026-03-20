// ─────────────────────────────────────────────────────────────────────────────
// xml_tests.cpp — IXmlDoc / IXmlNode 单元测试
// 使用 corekit 内置的轻量测试框架（与 interface_tests.cpp 保持一致）
// 覆盖：增 / 删 / 改 / 查 / 文件 IO / 路径查询 / 边界条件
// ─────────────────────────────────────────────────────────────────────────────

#include "corekit/xml/i_xml_doc.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace corekit::xml;
using namespace corekit::api;

// ─────────────────────────────────────────────────────────────────────────────
// 宏定义
// ─────────────────────────────────────────────────────────────────────────────

#define CHECK(expr)                                                        \
  do {                                                                     \
    if (!(expr)) {                                                         \
      std::fprintf(stderr, "  CHECK FAILED: %s  (%s:%d)\n", #expr,       \
                   __FILE__, __LINE__);                                    \
      return false;                                                        \
    }                                                                      \
  } while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_NE(a, b) CHECK((a) != (b))
#define CHECK_TRUE(x)  CHECK(x)
#define CHECK_FALSE(x) CHECK(!(x))

// ─────────────────────────────────────────────────────────────────────────────
// 测试数据
// ─────────────────────────────────────────────────────────────────────────────

static const char* kSampleXml = R"(<?xml version="1.0"?>
<config version="1.0" env="test">
  <database host="localhost" port="5432">
    <name>mydb</name>
    <pool_size>10</pool_size>
  </database>
  <logging level="info">
    <file>/var/log/app.log</file>
  </logging>
</config>)";

// ─────────────────────────────────────────────────────────────────────────────
// 1. 增（Create）
// ─────────────────────────────────────────────────────────────────────────────

bool TestCreateRootNode() {
  auto doc  = CreateXmlDoc();
  auto root = doc->CreateRoot("project");
  CHECK_NE(root, nullptr);
  CHECK_EQ(root->Name(), std::string("project"));
  return true;
}

bool TestAppendChildren() {
  auto doc  = CreateXmlDoc();
  auto root = doc->CreateRoot("items");
  auto a    = root->AppendChild("item");
  auto b    = root->AppendChild("item");
  CHECK_NE(a, nullptr);
  CHECK_NE(b, nullptr);
  auto kids = root->Children("item");
  CHECK_EQ(kids.size(), 2u);
  return true;
}

bool TestSetAttrOnNewNode() {
  auto doc  = CreateXmlDoc();
  auto root = doc->CreateRoot("server");
  root->SetAttr("host", "127.0.0.1");
  root->SetAttr("port", "8080");
  CHECK_EQ(root->Attr("host"), std::string("127.0.0.1"));
  CHECK_EQ(root->Attr("port"), std::string("8080"));
  return true;
}

bool TestSetTextOnNode() {
  auto doc  = CreateXmlDoc();
  auto root = doc->CreateRoot("greeting");
  root->SetText("hello world");
  CHECK_EQ(root->Text(), std::string("hello world"));
  return true;
}

bool TestNestedChildren() {
  auto doc    = CreateXmlDoc();
  auto root   = doc->CreateRoot("root");
  auto level1 = root->AppendChild("level1");
  auto level2 = level1->AppendChild("level2");
  level2->SetAttr("val", "deep");
  CHECK_EQ(level2->Attr("val"), std::string("deep"));
  CHECK_EQ(level1->FirstChild("level2")->Attr("val"), std::string("deep"));
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. 查（Read）
// ─────────────────────────────────────────────────────────────────────────────

bool TestLoadFromString() {
  auto doc = CreateXmlDoc();
  auto st  = doc->LoadFromString(kSampleXml);
  CHECK_TRUE(st.ok());
  auto root = doc->Root();
  CHECK_NE(root, nullptr);
  CHECK_EQ(root->Name(), std::string("config"));
  return true;
}

bool TestReadRootAttributes() {
  auto doc = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  auto root = doc->Root();
  CHECK_EQ(root->Attr("version"), std::string("1.0"));
  CHECK_EQ(root->Attr("env"),     std::string("test"));
  CHECK_EQ(root->Attr("missing", "default"), std::string("default"));
  return true;
}

bool TestHasAttr() {
  auto doc = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  auto root = doc->Root();
  CHECK_TRUE(root->HasAttr("version"));
  CHECK_FALSE(root->HasAttr("nonexistent"));
  return true;
}

bool TestAllAttrs() {
  auto doc = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  auto attrs = doc->Root()->Attrs();
  CHECK_EQ(attrs.size(), 2u);
  CHECK_EQ(attrs[0].first, std::string("version"));
  CHECK_EQ(attrs[1].first, std::string("env"));
  return true;
}

bool TestReadTextContent() {
  auto doc = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  auto db   = doc->Root()->FirstChild("database");
  CHECK_NE(db, nullptr);
  auto name_node = db->FirstChild("name");
  CHECK_NE(name_node, nullptr);
  CHECK_EQ(name_node->Text(), std::string("mydb"));
  return true;
}

bool TestFirstChildByTag() {
  auto doc = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  auto db = doc->Root()->FirstChild("database");
  CHECK_NE(db, nullptr);
  CHECK_EQ(db->Name(), std::string("database"));
  CHECK_EQ(db->Attr("host"), std::string("localhost"));
  return true;
}

bool TestFirstChildNoTagReturnsFirst() {
  auto doc = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  auto first = doc->Root()->FirstChild();
  CHECK_NE(first, nullptr);
  CHECK_EQ(first->Name(), std::string("database"));
  return true;
}

bool TestNextSibling() {
  auto doc = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  auto db  = doc->Root()->FirstChild("database");
  CHECK_NE(db, nullptr);
  auto log = db->NextSibling("logging");
  CHECK_NE(log, nullptr);
  CHECK_EQ(log->Name(), std::string("logging"));
  CHECK_EQ(log->Attr("level"), std::string("info"));
  return true;
}

bool TestNextSiblingNoTag() {
  auto doc = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  auto next = doc->Root()->FirstChild()->NextSibling();
  CHECK_NE(next, nullptr);
  CHECK_EQ(next->Name(), std::string("logging"));
  return true;
}

bool TestChildrenList() {
  auto doc = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  auto kids = doc->Root()->Children();
  CHECK_EQ(kids.size(), 2u);
  CHECK_EQ(kids[0]->Name(), std::string("database"));
  CHECK_EQ(kids[1]->Name(), std::string("logging"));
  return true;
}

bool TestChildrenByTag() {
  auto doc = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  auto dbs = doc->Root()->Children("database");
  CHECK_EQ(dbs.size(), 1u);
  return true;
}

bool TestFirstChildNotFound() {
  auto doc = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  CHECK_EQ(doc->Root()->FirstChild("no_such"), nullptr);
  return true;
}

bool TestEmptyDocRoot() {
  auto doc = CreateXmlDoc();
  CHECK_EQ(doc->Root(), nullptr);
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. 改（Update）
// ─────────────────────────────────────────────────────────────────────────────

bool TestOverwriteAttr() {
  auto doc = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  auto db = doc->Root()->FirstChild("database");
  CHECK_NE(db, nullptr);
  db->SetAttr("port", "9999");
  CHECK_EQ(db->Attr("port"), std::string("9999"));
  return true;
}

bool TestAddNewAttr() {
  auto doc = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  auto root = doc->Root();
  root->SetAttr("new_key", "new_value");
  CHECK_TRUE(root->HasAttr("new_key"));
  CHECK_EQ(root->Attr("new_key"), std::string("new_value"));
  return true;
}

bool TestRemoveAttr() {
  auto doc = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  auto root = doc->Root();
  CHECK_TRUE(root->HasAttr("env"));
  root->RemoveAttr("env");
  CHECK_FALSE(root->HasAttr("env"));
  return true;
}

bool TestRemoveNonExistentAttrIsNoop() {
  auto doc  = CreateXmlDoc();
  auto root = doc->CreateRoot("r");
  root->RemoveAttr("nonexistent");  // must not crash
  return true;
}

bool TestOverwriteText() {
  auto doc = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  auto pool = doc->Root()->FirstChild("database")->FirstChild("pool_size");
  CHECK_NE(pool, nullptr);
  pool->SetText("20");
  CHECK_EQ(pool->Text(), std::string("20"));
  return true;
}

bool TestCreateRootReplacesOld() {
  auto doc      = CreateXmlDoc();
  auto old_root = doc->CreateRoot("old");
  CHECK_EQ(old_root->Name(), std::string("old"));
  auto new_root = doc->CreateRoot("new");
  CHECK_EQ(new_root->Name(), std::string("new"));
  CHECK_EQ(doc->Root()->Name(), std::string("new"));
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. 删（Delete）
// ─────────────────────────────────────────────────────────────────────────────

bool TestRemoveFirstChild() {
  auto doc  = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  auto root = doc->Root();
  CHECK_TRUE(root->RemoveFirstChild("database"));
  auto kids = root->Children();
  CHECK_EQ(kids.size(), 1u);
  CHECK_EQ(kids[0]->Name(), std::string("logging"));
  return true;
}

bool TestRemoveFirstChildReturnsFalseWhenMissing() {
  auto doc  = CreateXmlDoc();
  auto root = doc->CreateRoot("r");
  CHECK_FALSE(root->RemoveFirstChild("nonexistent"));
  return true;
}

bool TestRemoveAllChildrenByTag() {
  auto doc  = CreateXmlDoc();
  auto root = doc->CreateRoot("list");
  root->AppendChild("item")->SetAttr("id", "1");
  root->AppendChild("item")->SetAttr("id", "2");
  root->AppendChild("other");
  root->RemoveAllChildren("item");
  auto kids = root->Children();
  CHECK_EQ(kids.size(), 1u);
  CHECK_EQ(kids[0]->Name(), std::string("other"));
  return true;
}

bool TestRemoveAllChildrenClearsAll() {
  auto doc  = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  auto root = doc->Root();
  root->RemoveAllChildren();
  CHECK_TRUE(root->Children().empty());
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 5. 路径查询 Find
// ─────────────────────────────────────────────────────────────────────────────

bool TestFindRoot() {
  auto doc = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  auto node = doc->Find("config");
  CHECK_NE(node, nullptr);
  CHECK_EQ(node->Name(), std::string("config"));
  return true;
}

bool TestFindNestedNode() {
  auto doc = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  auto node = doc->Find("config/database/name");
  CHECK_NE(node, nullptr);
  CHECK_EQ(node->Text(), std::string("mydb"));
  return true;
}

bool TestFindMissingReturnsNull() {
  auto doc = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  CHECK_EQ(doc->Find("config/no_such"), nullptr);
  return true;
}

bool TestFindWrongRootReturnsNull() {
  auto doc = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  CHECK_EQ(doc->Find("wrong_root"), nullptr);
  return true;
}

bool TestFindOnEmptyDocReturnsNull() {
  auto doc = CreateXmlDoc();
  CHECK_EQ(doc->Find("anything"), nullptr);
  return true;
}

bool TestFindEmptyPathReturnsNull() {
  auto doc = CreateXmlDoc();
  CHECK_TRUE(doc->LoadFromString(kSampleXml).ok());
  CHECK_EQ(doc->Find(""), nullptr);
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 6. 序列化 / 反序列化往返
// ─────────────────────────────────────────────────────────────────────────────

bool TestToStringAndBack() {
  auto doc  = CreateXmlDoc();
  auto root = doc->CreateRoot("app");
  root->SetAttr("version", "2.0");
  auto comp = root->AppendChild("component");
  comp->SetAttr("name", "engine");
  comp->SetText("main");

  std::string xml_str = doc->ToString();
  CHECK_FALSE(xml_str.empty());

  auto doc2 = CreateXmlDoc();
  CHECK_TRUE(doc2->LoadFromString(xml_str).ok());
  auto root2 = doc2->Root();
  CHECK_NE(root2, nullptr);
  CHECK_EQ(root2->Attr("version"), std::string("2.0"));
  CHECK_EQ(root2->FirstChild("component")->Text(), std::string("main"));
  return true;
}

bool TestSaveAndLoadFile() {
  const std::string path = "/tmp/corekit_xml_test_roundtrip.xml";

  auto doc  = CreateXmlDoc();
  auto root = doc->CreateRoot("data");
  root->SetAttr("id", "42");
  root->AppendChild("entry")->SetText("hello");
  CHECK_TRUE(doc->SaveToFile(path).ok());

  auto doc2  = CreateXmlDoc();
  CHECK_TRUE(doc2->LoadFromFile(path).ok());
  auto root2 = doc2->Root();
  CHECK_NE(root2, nullptr);
  CHECK_EQ(root2->Name(), std::string("data"));
  CHECK_EQ(root2->Attr("id"), std::string("42"));
  CHECK_EQ(root2->FirstChild("entry")->Text(), std::string("hello"));

  std::remove(path.c_str());
  return true;
}

bool TestLoadFromNonExistentFile() {
  auto doc = CreateXmlDoc();
  auto st  = doc->LoadFromFile("/tmp/this_file_does_not_exist_xyz.xml");
  CHECK_FALSE(st.ok());
  CHECK_EQ(st.code(), StatusCode::kNotFound);
  return true;
}

bool TestLoadFromInvalidXml() {
  auto doc = CreateXmlDoc();
  auto st  = doc->LoadFromString("<unclosed>");
  CHECK_FALSE(st.ok());
  CHECK_EQ(st.code(), StatusCode::kInvalidArgument);
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 7. 内存安全：节点可以比文档活得更久
// ─────────────────────────────────────────────────────────────────────────────

bool TestNodeOutlivesDoc() {
  XmlNodePtr root;
  {
    auto doc = CreateXmlDoc();
    doc->LoadFromString(kSampleXml);
    root = doc->Root();
    // doc goes out of scope here
  }
  CHECK_NE(root, nullptr);
  CHECK_EQ(root->Name(), std::string("config"));
  CHECK_EQ(root->Attr("version"), std::string("1.0"));
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main() {
  struct TestCase {
    const char* name;
    bool (*fn)();
  };

  const TestCase tests[] = {
      // 增
      {"XmlCreate.CreateRootNode",        TestCreateRootNode},
      {"XmlCreate.AppendChildren",        TestAppendChildren},
      {"XmlCreate.SetAttrOnNewNode",      TestSetAttrOnNewNode},
      {"XmlCreate.SetTextOnNode",         TestSetTextOnNode},
      {"XmlCreate.NestedChildren",        TestNestedChildren},
      // 查
      {"XmlRead.LoadFromString",          TestLoadFromString},
      {"XmlRead.ReadRootAttributes",      TestReadRootAttributes},
      {"XmlRead.HasAttr",                 TestHasAttr},
      {"XmlRead.AllAttrs",                TestAllAttrs},
      {"XmlRead.ReadTextContent",         TestReadTextContent},
      {"XmlRead.FirstChildByTag",         TestFirstChildByTag},
      {"XmlRead.FirstChildNoTagRetFirst", TestFirstChildNoTagReturnsFirst},
      {"XmlRead.NextSibling",             TestNextSibling},
      {"XmlRead.NextSiblingNoTag",        TestNextSiblingNoTag},
      {"XmlRead.ChildrenList",            TestChildrenList},
      {"XmlRead.ChildrenByTag",           TestChildrenByTag},
      {"XmlRead.FirstChildNotFound",      TestFirstChildNotFound},
      {"XmlRead.EmptyDocRoot",            TestEmptyDocRoot},
      // 改
      {"XmlUpdate.OverwriteAttr",         TestOverwriteAttr},
      {"XmlUpdate.AddNewAttr",            TestAddNewAttr},
      {"XmlUpdate.RemoveAttr",            TestRemoveAttr},
      {"XmlUpdate.RemoveNonExistAttrNoop",TestRemoveNonExistentAttrIsNoop},
      {"XmlUpdate.OverwriteText",         TestOverwriteText},
      {"XmlUpdate.CreateRootReplacesOld", TestCreateRootReplacesOld},
      // 删
      {"XmlDelete.RemoveFirstChild",      TestRemoveFirstChild},
      {"XmlDelete.RemoveFCReturnsFalse",  TestRemoveFirstChildReturnsFalseWhenMissing},
      {"XmlDelete.RemoveAllByTag",        TestRemoveAllChildrenByTag},
      {"XmlDelete.RemoveAllClearsAll",    TestRemoveAllChildrenClearsAll},
      // 路径查询
      {"XmlFind.FindRoot",                TestFindRoot},
      {"XmlFind.FindNestedNode",          TestFindNestedNode},
      {"XmlFind.FindMissingReturnsNull",  TestFindMissingReturnsNull},
      {"XmlFind.FindWrongRootNull",       TestFindWrongRootReturnsNull},
      {"XmlFind.FindOnEmptyDocNull",      TestFindOnEmptyDocReturnsNull},
      {"XmlFind.FindEmptyPathNull",       TestFindEmptyPathReturnsNull},
      // 序列化
      {"XmlSerialize.ToStringAndBack",    TestToStringAndBack},
      {"XmlSerialize.SaveAndLoadFile",    TestSaveAndLoadFile},
      {"XmlSerialize.LoadNonExistent",    TestLoadFromNonExistentFile},
      {"XmlSerialize.LoadInvalidXml",     TestLoadFromInvalidXml},
      // 内存安全
      {"XmlMemory.NodeOutlivesDoc",       TestNodeOutlivesDoc},
  };

  int passed = 0;
  int failed = 0;
  const int total = static_cast<int>(sizeof(tests) / sizeof(tests[0]));

  std::printf("[==========] Running %d xml tests\n", total);
  for (int i = 0; i < total; ++i) {
    std::printf("[RUN      ] %s\n", tests[i].name);
    std::fflush(stdout);
    bool ok = tests[i].fn();
    std::printf("%s %s\n", ok ? "[       OK ]" : "[  FAILED  ]", tests[i].name);
    std::fflush(stdout);
    if (ok) ++passed; else ++failed;
  }

  std::printf("[==========] %d tests ran.\n", total);
  std::printf("[  PASSED  ] %d tests.\n", passed);
  if (failed > 0) {
    std::printf("[  FAILED  ] %d tests.\n", failed);
  }
  return failed == 0 ? 0 : 1;
}
