#ifndef FORGE_LEXER_H_INCLUDED
#define FORGE_LEXER_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FORGE_LEXER_C_KEYWORDS {                \
                "#define",                      \
                "#ifndef",                      \
                "#ifdef",                       \
                "#endif",                       \
                "#if",                          \
                "#else",                        \
                "#include",                     \
                "alignas",                      \
                "alignof",                      \
                "auto",                         \
                "bool",                         \
                "break",                        \
                "case",                         \
                "char",                         \
                "const",                        \
                "constexpr",                    \
                "continue",                     \
                "default",                      \
                "do",                           \
                "double",                       \
                "else",                         \
                "enum",                         \
                "extern",                       \
                "false",                        \
                "float",                        \
                "for",                          \
                "goto",                         \
                "if",                           \
                "inline",                       \
                "int",                          \
                "long",                         \
                "nullptr",                      \
                "register",                     \
                "restrict",                     \
                "return",                       \
                "short",                        \
                "signed",                       \
                "sizeof",                       \
                "static",                       \
                "static_assert",                \
                "struct",                       \
                "switch",                       \
                "thread_local",                 \
                "true",                         \
                "typedef",                      \
                "typeof",                       \
                "typeof_unqual",                \
                "union",                        \
                "unsigned",                     \
                "void",                         \
                "volatile",                     \
                "while",                        \
                "_Alignas",                     \
                "_Alignof",                     \
                "_Atomic",                      \
                "_BitInt",                      \
                "_Bool",                        \
                "_Complex",                     \
                "_Decimal128",                  \
                "_Decimal32",                   \
                "_Decimal6",                    \
                "_Generic",                     \
                "_Imaginary",                   \
                "_Noreturn",                    \
                "_Static_assert",               \
                "_Thread_local",                \
                NULL,                           \
        }

#define FORGE_LEXER_PY_KEYWORDS {               \
                "False",                        \
                "None",                         \
                "True",                         \
                "and",                          \
                "as",                           \
                "assert",                       \
                "async",                        \
                "await",                        \
                "break",                        \
                "class",                        \
                "continue",                     \
                "def",                          \
                "del",                          \
                "elif",                         \
                "else",                         \
                "except",                       \
                "finally",                      \
                "for",                          \
                "from",                         \
                "global",                       \
                "if",                           \
                "import",                       \
                "in",                           \
                "is",                           \
                "lambda",                       \
                "nonlocal",                     \
                "not",                          \
                "or",                           \
                "pass",                         \
                "raise",                        \
                "return",                       \
                "try",                          \
                "while",                        \
                "with",                         \
                "yield",                        \
        }

typedef enum {
        FORGE_TOKEN_TYPE_EOF = 0,
        FORGE_TOKEN_TYPE_IDENTIFIER,
        FORGE_TOKEN_TYPE_KEYWORD,
        FORGE_TOKEN_TYPE_INTEGER_LITERAL,
        FORGE_TOKEN_TYPE_STRING_LITERAL,
        FORGE_TOKEN_TYPE_CHARACTER_LITERAL,

        // Optional
        FORGE_TOKEN_TYPE_NEWLINE,
        FORGE_TOKEN_TYPE_TAB,
        FORGE_TOKEN_TYPE_SPACE,

        // Symbols
        FORGE_TOKEN_TYPE_LEFT_PARENTHESIS,
        FORGE_TOKEN_TYPE_RIGHT_PARENTHESIS,
        FORGE_TOKEN_TYPE_LEFT_CURLY,
        FORGE_TOKEN_TYPE_RIGHT_CURLY,
        FORGE_TOKEN_TYPE_LEFT_SQUARE,
        FORGE_TOKEN_TYPE_RIGHT_SQUARE,
        FORGE_TOKEN_TYPE_BACKTICK,
        FORGE_TOKEN_TYPE_TILDE,
        FORGE_TOKEN_TYPE_BANG,
        FORGE_TOKEN_TYPE_AT,
        FORGE_TOKEN_TYPE_HASH,
        FORGE_TOKEN_TYPE_DOLLAR,
        FORGE_TOKEN_TYPE_PERCENT,
        FORGE_TOKEN_TYPE_UPTICK,
        FORGE_TOKEN_TYPE_AMPERSAND,
        FORGE_TOKEN_TYPE_ASTERISK,
        FORGE_TOKEN_TYPE_HYPHEN,
        FORGE_TOKEN_TYPE_PLUS,
        FORGE_TOKEN_TYPE_EQUALS,
        FORGE_TOKEN_TYPE_PIPE,
        FORGE_TOKEN_TYPE_BACKSLASH,
        FORGE_TOKEN_TYPE_FORWARDSLASH,
        FORGE_TOKEN_TYPE_LESSTHAN,
        FORGE_TOKEN_TYPE_GREATERTHAN,
        FORGE_TOKEN_TYPE_COMMA,
        FORGE_TOKEN_TYPE_PERIOD,
        FORGE_TOKEN_TYPE_QUESTION,
        FORGE_TOKEN_TYPE_SEMICOLON,
} forge_token_type;

typedef struct forge_token {
        char *lx;
        forge_token_type ty;
        struct {
                size_t r;
                size_t c;
                char *fp;
        } loc;
        struct forge_token *n;
} forge_token;

#define FORGE_LEXER_NO_BITS        (0)
#define FORGE_LEXER_TRACK_NEWLINES (1 << 0)
#define FORGE_LEXER_TRACK_TABS     (1 << 1)
#define FORGE_LEXER_TRACK_SPACES   (1 << 2)
#define FORGE_LEXER_CHARS_AS_STRS  (1 << 3)

typedef struct {
        const char *fp;
        const char *src;
        struct {
                const char *single;
                const char *multi_start;
                const char *multi_end;
        } comment;
        const char **kwds;
        uint32_t bits;
} forge_lexer_config;

typedef struct {
        forge_token *st; // DO NOT MODIFY!
        forge_token *hd;
        forge_token *tl;
        size_t sz;
        char *fp;
        struct {
                const char *msg;
                size_t r;
                size_t c;
        } err;
} forge_lexer;

/**
 * Parameter: config -> the config structure
 * Returns: a lexer of the content of `fp` tokenized
 * Description: Perform a lexical analysis of `config.fp`.
 *              An example of calling this function to
 *              parse a C file would be:
 *
 * === BEGIN SRC ===
 *      const char *kwds[] = FORGE_LEXER_C_KEYWORDS;
 *      const char *src = forge_io_forge_io_read_file_to_cstr(\"my_file.c\");
 *
 *      forge_lexer lexer = forge_lexer_create((forge_lexer_config){
 *              .fp = "/path/to/the/file.c",
 *              .src = src,
 *              .comment = {
 *                      .single      = "//",
 *                      .multi_start = "/\*",
 *                      .multi_end   = "*\/",
 *              },
 *              .kwds = (const char **)kwds,
 *              .bits = 0x0000,
 *      });
 *      if (forge_lexer_has_err(&lexer)) {
 *              printf("%s:%zu:%zu: %s\n", lexer.fp, lexer.err.r, lexer.err.c, lexer.err.msg);
 *      } else {
 *              forge_lexer_dump(&lexer);
 *      }
 *      forge_lexer_destroy(&lexer);
 *      free(src);
 *
 * === END SRC ===
 *
 *              If you want to enable lexing things like tabs, spaces,
 *              newlines etc., you can enable them with the
 *              config.bits field. For example, if you want to lex a
 *              Python file, you probably want:
 *
 *              config.bits = FORGE_LEXER_TRACK_NEWLINES
 *                            | FORGE_LEXER_TRACK_TABS
 *                            | FORGE_LEXER_CHARS_AS_STRS;
 */
forge_lexer forge_lexer_create(forge_lexer_config config);

/**
 * Parameter: fl -> the lexer
 * Description: Destroy all memory allocated by the lexer.
 */
void forge_lexer_destroy(forge_lexer *fl);

/**
 * Parameter: fl -> the lexer
 * Description: Print the tokenized content
 *              in the lexer.
 */
void forge_lexer_dump(const forge_lexer *fl);

/**
 * Parameter: fl -> the lexer
 * Returns: 1 if it encountered an error, and 0 if otherwise
 * Description: Check if the lexer encountered an error.
 */
int forge_lexer_has_err(const forge_lexer *fl);

/**
 * Parameter: fl -> the lexer
 * Returns: A properly formatted error message
 * Description: If the lexer has encountered an error,
 *              use this function to make a proper message
 *              with the filepath, line number, etc. This
 *              function will return NULL if there is no
 *              error that was encountered. The result
 *              needs to be free()'d.
 */
char *forge_lexer_format_err(const forge_lexer *fl);

/**
 * Parameter: fl -> the lexer
 * Parameter: ty -> the token type to expect
 * Returns: the token if the next token matches ty, and NULL if not
 * Description: Checks to see if the next token in the lexer
 *              matches `ty`.
 */
forge_token *forge_lexer_expect(forge_lexer *fl, forge_token_type ty);

#ifdef __cplusplus
}
#endif

#endif // FORGE_LEXER_H_INCLUDED
