#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>
#include <cstddef>

enum class TokenType {
    PRI,
    IF,
    ELSE,
    ELSE_IF,
    AND,
    OR,
    NOT,
    WHILE,
    DO,
    LET,
    PTR,
    RETURN,
    U1,
    U8,
    U16,
    U32,
    U64,
    I1,
    I8,
    I16,
    I32,
    I64,
    F32,
    F64,
    BOOL,
    CHAR,
    FOR,
    BREAK,
    CONTINUE,
    STRING, // the type name: string
    REQUIRES,
    ENSURES,
    INVARIANT,
    ASSERT,
    ASSUME,
    RESULT,
    IDENTIFIER,
    NUMBER, // integer literal
    FLOAT_LITERAL, // NEW: 3.14
    STRING_LITERAL, // NEW: "hello"
    CHAR_LITERAL, // NEW: 'a'
    TRUE_KW, // NEW
    FALSE_KW, // NEW
    EXTERN, // NEW
    NEW, // NEW
    DELETE_KW, // NEW (DELETE clashes with a Windows macro)
    PLUS,
    MINUS,
    MULTIPLY,
    DIVIDE,
    PERCENT, // NEW
    EQUAL,
    GREATER,
    LESS,
    GREATER_EQUAL,
    LESS_EQUAL,
    EQUAL_EQUAL,
    NOT_EQUAL,
    BANG, // NEW: single !
    ARROW,
    COLON,
    COMMA,
    DOT, // NEW
    AMPERSAND,
    PIPE,
    CARET,
    SHIFT_LEFT,
    SHIFT_RIGHT,
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET, // NEW
    RBRACKET, // NEW
    VERIFY_START, // now "#["
    VERIFY_END, // now "]#"
    SEMICOLON,
    END_OF_FILE,
    UNKNOWN,
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;
    std::size_t offset;
};

class Lexer {
public:
    explicit Lexer(std::string source);
    std::vector<Token> tokenize();

private:
    std::string source;
    size_t cursor = 0;
    int currentLine = 1;
    int currentColumn = 1;

    char peek() const;
    char peekNext() const; // NEW
    char advance();
    void skipWhitespace(); // now also skips // and /* */ comments
    char readEscape(); // NEW: after a backslash
};

#endif
