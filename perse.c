#include "compiler.h"
#include <stdlib.h>
#include <string.h>

LVar *locals;

void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

void error_at(char *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    int pos = loc - user_input;
    fprintf(stderr, "%s\n", user_input);
    fprintf(stderr, "%*s", pos, " ");
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

bool consume(char *op) {
    if (token->kind != TK_RESERVED || strlen(op) != token->len || memcmp(token->str, op, token->len))
        return false;
    token = token->next;
    return true;
}

void expect(char *op) {
    if (token->kind != TK_RESERVED || strlen(op) != token->len || memcmp(token->str, op, token->len))
        error_at(token->str, "'%s'ではありません", op);
    token = token->next;
}

bool equal(Token *tok, char *op) {
    return memcmp(tok->str, op, tok->len) == 0 && op[tok->len] == '\0';
}

int expect_number() {
    if (token->kind != TK_NUM)
        error_at(token->str, "数ではありません");
    int val = token->val;
    token = token->next;
    return val;
}

LVar *find_lvar(Token *tok) {
    for (LVar *var = locals; var; var = var->next)
        if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
            return var;
    return NULL;
}

bool at_eof() {
    return token->kind == TK_EOF;
}

bool at_return() {
    return token->kind == TK_RETURN;
}

int is_alnum(char c) {
    return ('a' <= c && c <= 'z') ||
           ('A' <= c && c <= 'Z') ||
           ('0' <= c && c <= '9') ||
           (c == '_');
}

int get_offset() {
    LVar *lvar = find_lvar(token);
    int offset;
    if (lvar) {
        offset = lvar->offset;
    } else {
        lvar = calloc(1, sizeof(LVar));
        lvar->next = locals;
        lvar->name = token->str;
        lvar->len = token->len;
        offset = locals->offset + 8;
        lvar->offset = offset;
        locals = lvar;
    }
    token = token->next;
    return offset;
}

char *reserved_words[] = {
    "==",
    "!=",
    "<=",
    ">=",
    "if",
    "else",
    "while",
    "for",
};

int get_reserved_len(char *p) {
    for (int i = 0; i < 8; i++) {
        if (strncmp(p, reserved_words[i], strlen(reserved_words[i])))
                return strlen(reserved_words[i]);
    }
    return 0;
}

Token *new_token(TokenKind kind, Token *cur, char *str, int len) {
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
    tok->len = len;
    cur->next = tok;
    return tok;
}

bool startswith(char *p, char *q) {
    return memcmp(p, q, strlen(q)) == 0;
}

Token *tokenize(char *p) {
    Token head;
    head.next = NULL;
    Token *cur = &head;

    while (*p) {
        if (isspace(*p)) {
            p++;
            continue;
        }

        if (startswith(p, "if")) {
            cur = new_token(TK_RESERVED, cur, p, 2);
            p += 2;
            continue;
        }

        if (startswith(p, "while")) {
            cur = new_token(TK_RESERVED, cur, p, 5);
            p += 2;
            continue;
        }

        if (startswith(p, "for")) {
            cur = new_token(TK_RESERVED, cur, p, 3);
            p += 3;
            continue;
        }

        if (startswith(p, "else")) {
            cur = new_token(TK_RESERVED, cur, p, 4);
            p += 4;
            continue;
        }

        if (startswith(p, "==") || startswith(p, "!=") || startswith(p, "<=") || startswith(p, ">=")) {
            cur = new_token(TK_RESERVED, cur, p, 2);
            p += 2;
            continue;
        }

        if (strchr("+-*/()<>=;", *p)) {
            cur = new_token(TK_RESERVED, cur, p++, 1);
            continue;
        }

        if (strncmp(p, "return", 6) == 0 && !is_alnum(p[6])) {
            cur = new_token(TK_RETURN, cur, p, 6);
            p += 6;
            continue;
        }

        if ('a' <= *p && *p <= 'z') {
            int len = 1;
            while ('a' <= *(p + len) && *(p + len) <= 'z')
                len++;

            cur = new_token(TK_IDENT, cur, p, len);
            p += cur->len;
            continue;
        }

        if (isdigit(*p)) {
            cur = new_token(TK_NUM, cur, p, 0);
            char *q = p;
            cur->val = strtol(p, &p, 10);
            cur->len = p - q;
            continue;
        }

        error_at(token->str, "トークナイズできません");
    }

    new_token(TK_EOF, cur, p, 0);
    return head.next;
}

Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

Node *new_node_num(int val) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->val = val;
    return node;
}

Node *new_node_ident(int offset) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_LVAR;

    node->offset = offset;
    return node;;
}

Node *code[100];

void *program() {
    int i = 0;
    locals = calloc(1, sizeof(LVar));
    locals->offset = 0;

    while (!at_eof())
        code[i++] = stmt();

    code[i] = NULL;
}

Node *stmt() {
    Node *node;
    
    if (consume("if")) {
        expect("(");
        Node *condition_node = expr();
        expect(")");
        node = stmt();
        if(consume("else"))
            node = new_node(ND_ELSE, node, stmt());
        
        node = new_node(ND_IF, condition_node, node);
        return node;
    } else if (consume("while")) {
        expect("(");
        node = expr();
        expect(")");
        node = new_node(ND_WHILE, node, stmt());
        return node;
    } else if (consume("for")) {
        Node *initial_node, *
        expect("(");
        if (!equal(token, ";"))
            node = new_node(ND_FOR, node, expr());
        if (!consume(";"))
            error_at(token->str, "';'ではないトークンです");

        if (!equal(token, ";"))
            node = new_node(ND_FOR, node, expr());

        if (!consume(";"))
            error_at(token->str, "';'ではないトークンです");

        if (!equal(token, ")"))
            node = new_node(ND_FOR, node, expr());

        if (!consume(")"))
            error_at(token->str, "')'ではないトークンです");

        
    } else if (at_return()) {
        node = calloc(1, sizeof(Node));
        node->kind = ND_RETURN;
        token = token->next;
        node->lhs = expr();
    } else {
        node = expr();
    }
    
    if (!consume(";"))
        error_at(token->str, "';'ではないトークンです");
    return node;
}

Node *expr() {
    Node *node = assign();
    return node;
}

Node *assign() {
    Node *node = equality();
    
    if (consume("=")) {
        node = new_node(ND_ASSIGN, node, assign());
    }
    return node;
}

Node *equality() {
    Node *node = relational();

    for (;;) {
        if (consume("=="))
            node = new_node(ND_EQ, node, relational());
        else if (consume("!="))
            node = new_node(ND_NE, node, relational());
        else
            return node;
    }
}

Node *relational() {
    Node *node = add();

    for (;;) {
        if (consume("<"))
            node = new_node(ND_LT, node, add());
        else if (consume("<="))
            node = new_node(ND_LE, node, add());
        else if (consume(">"))
            node = new_node(ND_LT, add(), node);
        else if (consume(">="))
            node = new_node(ND_LE, add(), node);
        else
            return node;
    }
}

Node *add() {
    Node *node = mul();

    for (;;) {
        if (consume("+"))
            node = new_node(ND_ADD, node, mul());
        else if (consume("-"))
            node = new_node(ND_SUB, node, mul());
        else
            return node;
    }
}

Node *mul() {
    Node *node = unary();

    for (;;) {
        if (consume("*"))
            node = new_node(ND_MUL, node, unary());
        else if (consume("/"))
            node = new_node(ND_DIV, node, unary());
        else
            return node;
    }
}

Node *unary() {
    if (consume("+"))
        return unary();
    if (consume("-"))
        return new_node(ND_SUB, new_node_num(0), unary());
    return primary();
}

Node *primary() {
    if (consume("(")) {
        Node *node = expr();
        expect(")");
        return node;
    }
    
    if (token->kind == TK_IDENT) {
        return new_node_ident(get_offset());
    }

    return new_node_num(expect_number());
}
