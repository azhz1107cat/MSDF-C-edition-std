# MSDF-C-edition-stb

msdf 是一个 单头文件、无外部依赖 的 C 语言库，用于解析缩进式配置文件（类似 YAML 的子集）。
设计灵感来自 yaml（缩进格式数据），适合用于游戏配置、场景描述等场景。

特性

- ✅ 基于缩进（空格）的层级结构
- ✅ 节点和键值属性
- ✅ # 行注释(要顶格)
- ✅ 内存安全（自动释放树结构）
- ✅ 简单的错误报告（行号 + 消息）
- ✅ stb 风格：单头文件，#define MSDF_IMPLEMENTATION 后包含即可使用
- ❌ 暂不支持：字面量类型区分（true/false/数字/数组），所有值均保存为字符串

语法示例

```msdf
# 这是一个注释
Screen:
    Button:
        text = "OK"
        x = 100
        y = 200
        visible = true      # 值保留原字符串 "true"
    Text title:
        size = 24
        color = "#FF0000"
```

API 文档

数据类型

```c
typedef struct MsdfAttribute {
    char *key;
    char *value;   // 原始字符串，未解析类型
} MsdfAttribute;

typedef struct MsdfNode {
    char *name;
    MsdfAttribute *attrs;
    size_t attrs_cnt;
    struct MsdfNode **children;
    size_t children_cnt;
} MsdfNode;

typedef struct {
    MsdfNode **ptr;
    size_t cnt;
} MsdfRoot;

typedef struct {
    bool has_error;
    int error_line;
    char *error_msg;   // 动态分配，需调用者 free
    MsdfRoot root;
} MsdfParseResult;
```

函数

```c
MsdfParseResult msdf_parse(const char *input);
```

解析整个配置文件，返回结果。

```c
void msdf_print(MsdfRoot root);
```

打印树结构（调试用）。

```c
void msdf_free(MsdfRoot root);
```

释放整个树的内存。

```c
void msdf_free_node(MsdfNode *n);
```

释放单个节点（通常不直接调用）。

使用示例

```c
#define MSDF_IMPLEMENTATION
#include "msdf.h"

int main() {
    const char *doc =
        "Game:\n"
        "    title = \"My Game\"\n"
        "    version = 1.0\n"
        "    Window:\n"
        "        width = 800\n"
        "        height = 600\n";

    MsdfParseResult res = msdf_parse(doc);
    if (res.has_error) {
        fprintf(stderr, "Error at line %d: %s\n", res.error_line, res.error_msg);
        msdf_free(res.root);
        free(res.error_msg);
        return 1;
    }

    msdf_print(res.root);
    msdf_free(res.root);
    return 0;
}
```

编译与集成

- 将 msdf.h 放入项目。
- 在恰好一个 .c 文件中定义 MSDF_IMPLEMENTATION。
- 其他文件直接 #include "msdf.h" 即可使用 API。
- 需要 C99 或更高版本。

未来版本可能扩展支持类型推断和数组。

---

EBNF 语法规范

以下 EBNF 描述了当前 msdf 解析器实际接受的语法。

```ebnf
(* 根文档由零个或多个顶层组件组成 *)
document     = { component } ;

(* 组件 = 节点名 + 冒号 + 缩进子块 *)
component    = node_name ":" newline indent_block ;
indent_block = { indent component | indent attribute } ;

(* 节点名：非空，不含冒号和控制字符，自动去除首尾空格 *)
node_name    = ? 任意非空字符序列（不含 ':' 和换行）? ;

(* 属性：键 = 值（值一直延伸到行尾，保留原始字符） *)
attribute    = key "=" value ;
key          = ? 属性名（去除首尾空格）? ;
value        = ? 行剩余部分（去除首尾空格）? ;

(* 缩进：一个或多个空格，用于表示层级 *)
indent       = " " { " " } ;

(* 注释行：以 # 开头，整行忽略 *)
comment      = "#" ? 任意字符 ? newline ;

(* 空行忽略 *)
empty_line   = ? 仅空白字符 ? newline ;

(* 行结束符 *)
newline      = ? 换行符（\n 或 \r\n）? ;
```

说明

- 解析器不区分字面量类型：所有属性值均以原始字符串存储（包括引号、数字等）。
- 缩进仅使用空格，制表符（\t）会破坏缩进计算（视为普通空白，但可能导致层级错误）。
- 节点名和属性名中的首尾空格会被自动删除。
- 属性值末尾的换行符不包含在值内。
- 重复的同名节点允许，解析器会保留多个子节点。

无效语法示例

- 属性出现在任何节点之外。
- 节点名空（如 : content）。
- 缩进不一致（如混合使用制表符和空格，或跳跃式缩进）。
- 既不是节点也不是属性的行（如裸字符串）。

---