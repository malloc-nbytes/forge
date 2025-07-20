#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "forge/lexer.h"
#include "forge/io.h"
#include "forge/mem.h"
#include "forge/smap.h"
#include "forge/str.h"

static const char *
forge_token_type_to_cstr(forge_token_type ty)
{
        switch (ty) {
        case FORGE_TOKEN_TYPE_EOF:               return "FORGE_TOKEN_TYPE_EOF";
        case FORGE_TOKEN_TYPE_IDENTIFIER:        return "FORGE_TOKEN_TYPE_IDENTIFIER";
        case FORGE_TOKEN_TYPE_KEYWORD:           return "FORGE_TOKEN_TYPE_KEYWORD";
        case FORGE_TOKEN_TYPE_INTEGER_LITERAL:   return "FORGE_TOKEN_TYPE_INTEGER_LITERAL";
        case FORGE_TOKEN_TYPE_STRING_LITERAL:    return "FORGE_TOKEN_TYPE_STRING_LITERAL";
        case FORGE_TOKEN_TYPE_CHARACTER_LITERAL: return "FORGE_TOKEN_TYPE_CHARACTER_LITERAL";

        // Optional
        case FORGE_TOKEN_TYPE_NEWLINE:           return "FORGE_TOKEN_TYPE_NEWLINE";
        case FORGE_TOKEN_TYPE_TAB:               return "FORGE_TOKEN_TYPE_TAB";
        case FORGE_TOKEN_TYPE_SPACE:             return "FORGE_TOKEN_TYPE_SPACE";

        // Symbols
        case FORGE_TOKEN_TYPE_LEFT_PARENTHESIS:  return "FORGE_TOKEN_TYPE_LEFT_PARENTHESIS";
        case FORGE_TOKEN_TYPE_RIGHT_PARENTHESIS: return "FORGE_TOKEN_TYPE_RIGHT_PARENTHESIS";
        case FORGE_TOKEN_TYPE_LEFT_CURLY:        return "FORGE_TOKEN_TYPE_LEFT_CURLY";
        case FORGE_TOKEN_TYPE_RIGHT_CURLY:       return "FORGE_TOKEN_TYPE_RIGHT_CURLY";
        case FORGE_TOKEN_TYPE_LEFT_SQUARE:       return "FORGE_TOKEN_TYPE_LEFT_SQUARE";
        case FORGE_TOKEN_TYPE_RIGHT_SQUARE:      return "FORGE_TOKEN_TYPE_RIGHT_SQUARE";
        case FORGE_TOKEN_TYPE_BACKTICK:          return "FORGE_TOKEN_TYPE_BACKTICK";
        case FORGE_TOKEN_TYPE_TILDE:             return "FORGE_TOKEN_TYPE_TILDE";
        case FORGE_TOKEN_TYPE_BANG:              return "FORGE_TOKEN_TYPE_BANG";
        case FORGE_TOKEN_TYPE_AT:                return "FORGE_TOKEN_TYPE_AT";
        case FORGE_TOKEN_TYPE_HASH:              return "FORGE_TOKEN_TYPE_HASH";
        case FORGE_TOKEN_TYPE_DOLLAR:            return "FORGE_TOKEN_TYPE_DOLLAR";
        case FORGE_TOKEN_TYPE_PERCENT:           return "FORGE_TOKEN_TYPE_PERCENT";
        case FORGE_TOKEN_TYPE_UPTICK:            return "FORGE_TOKEN_TYPE_UPTICK";
        case FORGE_TOKEN_TYPE_AMPERSAND:         return "FORGE_TOKEN_TYPE_AMPERSAND";
        case FORGE_TOKEN_TYPE_ASTERISK:          return "FORGE_TOKEN_TYPE_ASTERISK";
        case FORGE_TOKEN_TYPE_HYPHEN:            return "FORGE_TOKEN_TYPE_HYPHEN";
        case FORGE_TOKEN_TYPE_PLUS:              return "FORGE_TOKEN_TYPE_PLUS";
        case FORGE_TOKEN_TYPE_EQUALS:            return "FORGE_TOKEN_TYPE_EQUALS";
        case FORGE_TOKEN_TYPE_PIPE:              return "FORGE_TOKEN_TYPE_PIPE";
        case FORGE_TOKEN_TYPE_BACKSLASH:         return "FORGE_TOKEN_TYPE_BACKSLASH";
        case FORGE_TOKEN_TYPE_FORWARDSLASH:      return "FORGE_TOKEN_TYPE_FORWARDSLASH";
        case FORGE_TOKEN_TYPE_LESSTHAN:          return "FORGE_TOKEN_TYPE_LESSTHAN";
        case FORGE_TOKEN_TYPE_GREATERTHAN:       return "FORGE_TOKEN_TYPE_GREATERTHAN";
        case FORGE_TOKEN_TYPE_COMMA:             return "FORGE_TOKEN_TYPE_COMMA";
        case FORGE_TOKEN_TYPE_PERIOD:            return "FORGE_TOKEN_TYPE_PERIOD";
        case FORGE_TOKEN_TYPE_QUESTION:          return "FORGE_TOKEN_TYPE_QUESTION";
        case FORGE_TOKEN_TYPE_SEMICOLON:         return "FORGE_TOKEN_TYPE_SEMICOLON";
        default: {
                fprintf(stderr, "invalid token: %d\n", (int)ty);
                exit(1);
        } break;
        }
        return NULL; // unreachable
}

static void
forge_token_free(forge_token *t)
{
        // do not free t->loc.fp, gets free()'d in forge_lexer_destroy().
        free(t->lx);
        free(t);
}

static forge_token *
forge_token_alloc(char            *st,
                  size_t           st_n,
                  forge_token_type ty,
                  size_t           r,
                  size_t           c,
                  char            *fp/*from lexer->fp*/)
{
        forge_token *t = (forge_token *)forge_mem_malloc(sizeof(forge_token));

        t->lx     = strndup(st, st_n);
        t->ty     = ty;
        t->loc.r  = r;
        t->loc.c  = c;
        t->loc.fp = fp;
        t->n      = NULL;

        return t;
}

static void
forge_lexer_append(forge_lexer *fl,
                   forge_token *t)
{
        if (!fl->st && !fl->hd && !fl->tl) {
                fl->st = fl->hd = fl->tl = t;
        } else {
                fl->tl->n = t;
                fl->tl = t;
        }
        fl->sz++;
}

static forge_smap
init_ops(void)
{
        forge_smap m = forge_smap_create();

        static const int init_ops_left_parenthesis  = FORGE_TOKEN_TYPE_LEFT_PARENTHESIS;
        static const int init_ops_right_parenthesis = FORGE_TOKEN_TYPE_RIGHT_PARENTHESIS;
        static const int init_ops_left_curly        = FORGE_TOKEN_TYPE_LEFT_CURLY;
        static const int init_ops_right_curly       = FORGE_TOKEN_TYPE_RIGHT_CURLY;
        static const int init_ops_left_square       = FORGE_TOKEN_TYPE_LEFT_SQUARE;
        static const int init_ops_right_square      = FORGE_TOKEN_TYPE_RIGHT_SQUARE;
        static const int init_ops_backtick          = FORGE_TOKEN_TYPE_BACKTICK;
        static const int init_ops_tilde             = FORGE_TOKEN_TYPE_TILDE;
        static const int init_ops_bang              = FORGE_TOKEN_TYPE_BANG;
        static const int init_ops_at                = FORGE_TOKEN_TYPE_AT;
        static const int init_ops_hash              = FORGE_TOKEN_TYPE_HASH;
        static const int init_ops_dollar            = FORGE_TOKEN_TYPE_DOLLAR;
        static const int init_ops_percent           = FORGE_TOKEN_TYPE_PERCENT;
        static const int init_ops_uptick            = FORGE_TOKEN_TYPE_UPTICK;
        static const int init_ops_ampersand         = FORGE_TOKEN_TYPE_AMPERSAND;
        static const int init_ops_asterisk          = FORGE_TOKEN_TYPE_ASTERISK;
        static const int init_ops_hyphen            = FORGE_TOKEN_TYPE_HYPHEN;
        static const int init_ops_plus              = FORGE_TOKEN_TYPE_PLUS;
        static const int init_ops_equals            = FORGE_TOKEN_TYPE_EQUALS;
        static const int init_ops_pipe              = FORGE_TOKEN_TYPE_PIPE;
        static const int init_ops_backslash         = FORGE_TOKEN_TYPE_BACKSLASH;
        static const int init_ops_forwardslash      = FORGE_TOKEN_TYPE_FORWARDSLASH;
        static const int init_ops_lessthan          = FORGE_TOKEN_TYPE_LESSTHAN;
        static const int init_ops_greaterthan       = FORGE_TOKEN_TYPE_GREATERTHAN;
        static const int init_ops_comma             = FORGE_TOKEN_TYPE_COMMA;
        static const int init_ops_period            = FORGE_TOKEN_TYPE_PERIOD;
        static const int init_ops_question          = FORGE_TOKEN_TYPE_QUESTION;
        static const int init_ops_semicolon         = FORGE_TOKEN_TYPE_SEMICOLON;

        forge_smap_insert(&m, "(",  (void *)&init_ops_left_parenthesis);
        forge_smap_insert(&m, ")",  (void *)&init_ops_right_parenthesis);
        forge_smap_insert(&m, "{",  (void *)&init_ops_left_curly);
        forge_smap_insert(&m, "}",  (void *)&init_ops_right_curly);
        forge_smap_insert(&m, "[",  (void *)&init_ops_left_square);
        forge_smap_insert(&m, "]",  (void *)&init_ops_right_square);
        forge_smap_insert(&m, "`",  (void *)&init_ops_backtick);
        forge_smap_insert(&m, "~",  (void *)&init_ops_tilde);
        forge_smap_insert(&m, "!",  (void *)&init_ops_bang);
        forge_smap_insert(&m, "@",  (void *)&init_ops_at);
        forge_smap_insert(&m, "#",  (void *)&init_ops_hash);
        forge_smap_insert(&m, "$",  (void *)&init_ops_dollar);
        forge_smap_insert(&m, "%",  (void *)&init_ops_percent);
        forge_smap_insert(&m, "^",  (void *)&init_ops_uptick);
        forge_smap_insert(&m, "&",  (void *)&init_ops_ampersand);
        forge_smap_insert(&m, "*",  (void *)&init_ops_asterisk);
        forge_smap_insert(&m, "-",  (void *)&init_ops_hyphen);
        forge_smap_insert(&m, "+",  (void *)&init_ops_plus);
        forge_smap_insert(&m, "=",  (void *)&init_ops_equals);
        forge_smap_insert(&m, "|",  (void *)&init_ops_pipe);
        forge_smap_insert(&m, "\\", (void *)&init_ops_backslash);
        forge_smap_insert(&m, "/",  (void *)&init_ops_forwardslash);
        forge_smap_insert(&m, "<",  (void *)&init_ops_lessthan);
        forge_smap_insert(&m, ">",  (void *)&init_ops_greaterthan);
        forge_smap_insert(&m, ",",  (void *)&init_ops_comma);
        forge_smap_insert(&m, ".",  (void *)&init_ops_period);
        forge_smap_insert(&m, "?",  (void *)&init_ops_question);
        forge_smap_insert(&m, ";",  (void *)&init_ops_semicolon);

        return m;
}

static size_t
consume_while(const char *st, int (*pred)(int))
{
        size_t i = 0;
        int skip = 0;
        while (st[i]) {
                if (!pred(st[i]) && !skip) {
                        break;
                } else if (st[i] == '\\') {
                        skip = 1;
                } else {
                        skip = 0;
                }
                ++i;
        }
        return i;
}

static int
iskw(char *s, const char **kwds)
{
        for (size_t i = 0; kwds[i]; ++i) {
                if (!strcmp(s, kwds[i])) {
                        return 1;
                }
        }
        return 0;
}

static int
isident(int c)
{
        return isalnum(c) || (char)c == '_';
}

static int
not_quote(int c)
{
        return (char)c != '"';
}

static int
not_single_quote(int c)
{
        return (char)c != '\'';
}

forge_lexer
forge_lexer_create(forge_lexer_config config)
{
        forge_lexer fl = {
                .st  = NULL,
                .hd  = NULL,
                .tl  = NULL,
                .sz  = 0,
                .fp  = NULL,
                .err = {
                        .msg = NULL,
                        .r = 0,
                        .c = 0,
                },
        };

        if (!forge_io_filepath_exists(config.fp)) {
                fl.err.msg = "filepath does not exist";
                return fl;
        }

        if (!config.comment.single || !config.comment.multi_start || !config.comment.multi_end) {
                fl.err.msg = "comment configuration is incomplete";
                return fl;
        }

        fl.fp = strdup(config.fp);

        forge_smap ops = init_ops();
        char *src      = strdup(config.src);
        size_t r       = 1;
        size_t c       = 1;
        size_t i       = 0;

        while (src[i]) {
                char ch = src[i];

                // Single-line comment
                if (strncmp(src + i, config.comment.single, strlen(config.comment.single)) == 0) {
                        i += strlen(config.comment.single);
                        c += strlen(config.comment.single);
                        while (src[i] && src[i] != '\n') {
                                ++i, ++c;
                        }
                        // If we hit a newline, it will be handled in the next iteration
                        continue;
                }
                // Multi-line comment
                else if (strncmp(src + i, config.comment.multi_start, strlen(config.comment.multi_start)) == 0) {
                        i += strlen(config.comment.multi_start);
                        c += strlen(config.comment.multi_start);
                        while (src[i] && strncmp(src + i, config.comment.multi_end, strlen(config.comment.multi_end)) != 0) {
                                if (src[i] == '\n') {
                                        ++r;
                                        c = 1;
                                } else {
                                        ++c;
                                }
                                ++i;
                        }
                        if (!src[i]) {
                                free(src);
                                forge_smap_destroy(&ops);
                                fl.err.msg = "unterminated multiline comment";
                                fl.err.r = r;
                                fl.err.c = c;
                                return fl;
                        }
                        i += strlen(config.comment.multi_end);
                        c += strlen(config.comment.multi_end);
                        continue;
                }
                else if (ch == ' ') {
                        if (config.bits & FORGE_LEXER_TRACK_SPACES) {
                                forge_token *t = forge_token_alloc(src + i, 1,
                                                                   FORGE_TOKEN_TYPE_SPACE,
                                                                   r, c, fl.fp);
                                forge_lexer_append(&fl, t);
                        }
                        ++i, ++c;
                }
                else if (ch == '\t') {
                        if (config.bits & FORGE_LEXER_TRACK_TABS) {
                                forge_token *t = forge_token_alloc(src + i, 1,
                                                                   FORGE_TOKEN_TYPE_TAB,
                                                                   r, c, fl.fp);
                                forge_lexer_append(&fl, t);
                        }
                        ++i, ++c;
                }
                else if (ch == '\n') {
                        if (config.bits & FORGE_LEXER_TRACK_NEWLINES) {
                                forge_token *t = forge_token_alloc(src + i, 1,
                                                                   FORGE_TOKEN_TYPE_NEWLINE,
                                                                   r, c, fl.fp);
                                forge_lexer_append(&fl, t);
                        }
                        ++i, ++r, c = 1;
                }
                else if (ch == '"') {
                        ++i, ++c;
                        size_t len = consume_while(src + i, not_quote);
                        if (src[i + len] != '"') {
                                free(src);
                                forge_smap_destroy(&ops);
                                fl.err.msg = "unterminated string literal";
                                fl.err.r = r;
                                fl.err.c = c;
                                return fl;
                        }
                        forge_token *t = forge_token_alloc(src + i, len,
                                                           FORGE_TOKEN_TYPE_STRING_LITERAL,
                                                           r, c, fl.fp);
                        forge_lexer_append(&fl, t);
                        i += len + 1, c += len + 1;
                }
                else if (ch == '\'') {
                        ++i, ++c;
                        if (config.bits & FORGE_LEXER_CHARS_AS_STRS) {
                                // Treat as string literal: consume until closing quote, handling escapes
                                size_t len = consume_while(src + i, not_single_quote);
                                if (src[i + len] != '\'') {
                                        free(src);
                                        forge_smap_destroy(&ops);
                                        fl.err.msg = "unterminated string literal";
                                        fl.err.r = r;
                                        fl.err.c = c;
                                        return fl;
                                }
                                forge_token *t = forge_token_alloc(src + i, len,
                                                                   FORGE_TOKEN_TYPE_STRING_LITERAL,
                                                                   r, c, fl.fp);
                                forge_lexer_append(&fl, t);
                                i += len + 1, c += len + 1;
                        } else {
                                // Original character literal handling
                                size_t len = 0;
                                if (src[i] == '\\') {
                                        if (src[i + 1] == 'n' || src[i + 1] == 't' || src[i + 1] == '\'' ||
                                            src[i + 1] == '\\' || src[i + 1] == 'r' || src[i + 1] == '0') {
                                                len = 2;
                                        } else {
                                                free(src);
                                                forge_smap_destroy(&ops);
                                                fl.err.msg = "invalid escape sequence in character literal";
                                                fl.err.r = r;
                                                fl.err.c = c;
                                                return fl;
                                        }
                                } else if (src[i] && src[i] != '\'') {
                                        len = 1;
                                } else {
                                        free(src);
                                        forge_smap_destroy(&ops);
                                        fl.err.msg = "empty character literal";
                                        fl.err.r = r;
                                        fl.err.c = c;
                                        return fl;
                                }
                                if (src[i + len] != '\'') {
                                        free(src);
                                        forge_smap_destroy(&ops);
                                        fl.err.msg = "unterminated character literal";
                                        fl.err.r = r;
                                        fl.err.c = c;
                                        return fl;
                                }
                                forge_token *t = forge_token_alloc(src + i - 1, len + 2,
                                                                   FORGE_TOKEN_TYPE_CHARACTER_LITERAL,
                                                                   r, c, fl.fp);
                                forge_lexer_append(&fl, t);
                                i += len + 1, c += len + 1;
                        }
                }
                else if (ch == '_' || isalpha(ch)) {
                        size_t len = consume_while(src + i, isident);
                        char *ident = strndup(src + i, len);
                        forge_token_type ty = iskw(ident, (const char **)config.kwds)
                                ? FORGE_TOKEN_TYPE_KEYWORD
                                : FORGE_TOKEN_TYPE_IDENTIFIER;
                        forge_token *t = forge_token_alloc(src + i, len, ty, r, c, fl.fp);
                        forge_lexer_append(&fl, t);
                        free(ident);
                        i += len, c += len;
                }
                else if (isdigit(ch)) {
                        size_t len = consume_while(src + i, isdigit);
                        forge_token *t = forge_token_alloc(src + i, len,
                                                           FORGE_TOKEN_TYPE_INTEGER_LITERAL,
                                                           r, c, fl.fp);
                        forge_lexer_append(&fl, t);
                        i += len, c += len;
                }
                else {
                        char op_str[2] = {ch, '\0'};
                        void *op_type = forge_smap_get(&ops, op_str);
                        if (op_type) {
                                forge_token *t = forge_token_alloc(src + i, 1,
                                                                   *(forge_token_type *)op_type,
                                                                   r, c, fl.fp);
                                forge_lexer_append(&fl, t);
                        } // Ignore unrecognized characters
                        ++i, ++c;
                }
        }

        free(src);
        forge_smap_destroy(&ops);

        forge_token *eof = forge_token_alloc("EOF", 3,
                                             FORGE_TOKEN_TYPE_EOF,
                                             r, c, fl.fp);
        forge_lexer_append(&fl, eof);

        return fl;
}

void
forge_lexer_dump(const forge_lexer *fl)
{
        forge_token *it = fl->hd;
        while (it) {
                printf("[lx = `%s`, ty = %s, r = %zu, c = %zu, fp = %s]\n",
                       it->lx, forge_token_type_to_cstr(it->ty),
                       it->loc.r, it->loc.c, it->loc.fp);
                it = it->n;
        }
}

void
forge_lexer_destroy(forge_lexer *fl)
{
        if (!fl) return;

        // Do not free fl->err, not alloc'd

        if (fl->fp) {
                free(fl->fp);
        }

        while (fl->st) {
                forge_token *tmp = fl->st;
                fl->st = fl->st->n;
                forge_token_free(tmp);
        }
}

int
forge_lexer_has_err(const forge_lexer *fl)
{
        return fl->err.msg != NULL;
}

char *
forge_lexer_format_err(const forge_lexer *fl)
{
        if (!forge_lexer_has_err(fl)) return NULL;

        char row[256] = {0};
        char col[256] = {0};

        sprintf(row, "%zu", fl->err.r);
        sprintf(col, "%zu", fl->err.c);

        return forge_str_builder(fl->fp, ":", row, ":", col, ":", fl->err.msg, NULL);
}
