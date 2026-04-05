#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

// ------------------------------
// 数据结构
// ------------------------------
typedef struct MsdfAttribute {
    char *key;
    char *value;
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
    char *error_msg;   // 动态分配
    MsdfRoot root;
} MsdfParseResult;

// ------------------------------
// 辅助函数
// ------------------------------
static void append_child(MsdfNode *parent, MsdfNode *child) {
    parent->children = realloc(parent->children, (parent->children_cnt + 1) * sizeof(MsdfNode*));
    parent->children[parent->children_cnt++] = child;
}

static void append_attr(MsdfNode *node, const char *key, const char *value) {
    node->attrs = realloc(node->attrs, (node->attrs_cnt + 1) * sizeof(MsdfAttribute));
    node->attrs[node->attrs_cnt].key = strdup(key);
    node->attrs[node->attrs_cnt].value = strdup(value);
    node->attrs_cnt++;
}

// ------------------------------
// 安全的释放函数
// ------------------------------
void msdf_free_node(MsdfNode *n) {
    if (!n) return;
    for (size_t i = 0; i < n->attrs_cnt; i++) {
        free(n->attrs[i].key);
        free(n->attrs[i].value);
    }
    free(n->attrs);
    if (n->children) {
        for (size_t i = 0; i < n->children_cnt; i++)
            msdf_free_node(n->children[i]);
        free(n->children);
    }
    free(n->name);
    free(n);
}

void msdf_free(MsdfRoot root) {
    if (root.ptr) {
        for (size_t i = 0; i < root.cnt; i++)
            msdf_free_node(root.ptr[i]);
        free(root.ptr);
    }
}

// ------------------------------
// 打印树
// ------------------------------
static void print_node(MsdfNode *n, int indent) {
    for (int i = 0; i < indent; i++) printf("  ");
    printf("- %s", n->name);
    for (size_t i = 0; i < n->attrs_cnt; i++)
        printf("  %s=%s", n->attrs[i].key, n->attrs[i].value);
    printf("\n");
    for (size_t i = 0; i < n->children_cnt; i++)
        print_node(n->children[i], indent + 1);
}

void msdf_print(MsdfRoot root) {
    for (size_t i = 0; i < root.cnt; i++)
        print_node(root.ptr[i], 0);
}

// ------------------------------
// 解析器核心
// ------------------------------
static int count_indent(const char *s) {
    int spaces = 0;
    while (*s == ' ') { spaces++; s++; }
    return spaces;
}

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

MsdfParseResult msdf_parse(const char *input) {
    MsdfParseResult res = {
        .has_error = false,
        .error_line = 0,
        .error_msg = NULL,
        .root = { .ptr = NULL, .cnt = 0 }
    };

    char *input_copy = strdup(input);
    if (!input_copy) {
        res.has_error = true;
        res.error_msg = strdup("Out of memory");
        return res;
    }

    // 用于维护父子关系的栈（存储当前路径上的节点）
    MsdfNode **stack = NULL;
    int *indent_stack = NULL;
    int stack_size = 0;
    int stack_cap = 0;

    char *line = strtok(input_copy, "\n");
    int line_num = 1;

    while (line) {
        // 跳过空行和注释
        char *trimmed = trim(line);
        if (trimmed[0] == '#' || trimmed[0] == '\0') {
            line = strtok(NULL, "\n");
            line_num++;
            continue;
        }

        int indent = count_indent(line);
        char *content = line + indent;   // 去除前导空格后的内容

        // 判断行类型：节点行（以冒号结尾）还是属性行（包含等号）
        bool is_node = (strchr(content, ':') != NULL);
        bool is_attr = (strchr(content, '=') != NULL);

        if (!is_node && !is_attr) {
            // 非法行
            res.has_error = true;
            res.error_line = line_num;
            res.error_msg = strdup("Unexpected line format");
            goto cleanup;
        }

        if (is_node) {
            // 节点行格式： "Name:"  或  "Name :"  去掉尾部空格
            char *colon = strchr(content, ':');
            *colon = '\0';
            char *node_name = trim(content);
            if (strlen(node_name) == 0) {
                res.has_error = true;
                res.error_line = line_num;
                res.error_msg = strdup("Empty node name");
                goto cleanup;
            }

            MsdfNode *new_node = calloc(1, sizeof(MsdfNode));
            new_node->name = strdup(node_name);

            // 根据缩进确定父节点
            // 弹出栈中缩进 >= 当前缩进的节点（回到正确的父层级）
            while (stack_size > 0 && indent_stack[stack_size-1] >= indent) {
                stack_size--;
            }

            if (stack_size == 0) {
                // 顶层节点，添加到 root
                res.root.ptr = realloc(res.root.ptr, (res.root.cnt + 1) * sizeof(MsdfNode*));
                res.root.ptr[res.root.cnt++] = new_node;
            } else {
                // 父节点是栈顶
                append_child(stack[stack_size-1], new_node);
            }

            // 将当前节点压栈
            if (stack_size >= stack_cap) {
                stack_cap = stack_cap ? stack_cap*2 : 8;
                stack = realloc(stack, stack_cap * sizeof(MsdfNode*));
                indent_stack = realloc(indent_stack, stack_cap * sizeof(int));
            }
            stack[stack_size] = new_node;
            indent_stack[stack_size] = indent;
            stack_size++;
        }
        else if (is_attr) {
            // 属性行格式： key = value
            char *eq = strchr(content, '=');
            *eq = '\0';
            char *key = trim(content);
            char *value = trim(eq+1);

            if (stack_size == 0) {
                res.has_error = true;
                res.error_line = line_num;
                res.error_msg = strdup("Attribute without parent node");
                goto cleanup;
            }

            MsdfNode *current = stack[stack_size-1];
            append_attr(current, key, value);
        }

        line = strtok(NULL, "\n");
        line_num++;
    }

cleanup:
    free(stack);
    free(indent_stack);
    free(input_copy);

    if (res.has_error) {
        msdf_free(res.root);
        res.root.ptr = NULL;
        res.root.cnt = 0;
    }
    return res;
}