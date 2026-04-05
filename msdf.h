/*
 * msdf.h - 简易的缩进式配置解析器
 * authors: azhz1107cat, deepseek
 * 用法：
 *   在**一个**C文件中定义 MSDF_IMPLEMENTATION 宏，然后包含此头文件。
 *   在其他需要接口的文件中，直接包含此头文件即可。
 *
 *   示例：
 *     #define MSDF_IMPLEMENTATION
 *     #include "msdf.h"
 *
 *     int main() {
 *         MsdfParseResult res = msdf_parse("Screen:\n    Button:\n        text = \"OK\"");
 *         if (!res.has_error) msdf_print(res.root);
 *         msdf_free(res.root);
 *         free(res.error_msg);
 *         return 0;
 *     }
 */

#ifndef MSDF_H_INCLUDED
#define MSDF_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

// ------------------------------
// 公共数据类型
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
    char *error_msg;   // 动态分配，需调用者释放
    MsdfRoot root;
} MsdfParseResult;

// ------------------------------
// 公共 API 声明
// ------------------------------
extern void msdf_free_node(MsdfNode *n);
extern void msdf_free(MsdfRoot root);
extern void msdf_print(MsdfRoot root);
extern MsdfParseResult msdf_parse(const char *input);

#ifdef __cplusplus
}
#endif

// ------------------------------
// 实现部分
// ------------------------------
#ifdef MSDF_IMPLEMENTATION

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

    MsdfNode **stack = NULL;
    int *indent_stack = NULL;
    int stack_size = 0;
    int stack_cap = 0;

    char *line = strtok(input_copy, "\n");
    int line_num = 1;

    while (line) {
        char *trimmed = trim(line);
        if (trimmed[0] == '#' || trimmed[0] == '\0') {
            line = strtok(NULL, "\n");
            line_num++;
            continue;
        }

        int indent = count_indent(line);
        char *content = line + indent;

        bool is_node = (strchr(content, ':') != NULL);
        bool is_attr = (strchr(content, '=') != NULL);

        if (!is_node && !is_attr) {
            res.has_error = true;
            res.error_line = line_num;
            res.error_msg = strdup("Unexpected line format");
            goto cleanup;
        }

        if (is_node) {
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

            while (stack_size > 0 && indent_stack[stack_size-1] >= indent) {
                stack_size--;
            }

            if (stack_size == 0) {
                res.root.ptr = realloc(res.root.ptr, (res.root.cnt + 1) * sizeof(MsdfNode*));
                res.root.ptr[res.root.cnt++] = new_node;
            } else {
                append_child(stack[stack_size-1], new_node);
            }

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

#endif // MSDF_IMPLEMENTATION

#endif // MSDF_H_INCLUDED