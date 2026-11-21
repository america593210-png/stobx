#include "Lexer.h"
#include <cctype>
#include <unordered_map>
#include <utility>

// NEW: keyword table replaces the long else-if chain.
// (else / else if are handled separately below.)
static const std::unordered_map<std::string, TokenType> keywords = {
    {"pri", TokenType::PRI},
    {"if", TokenType::IF},
    {"and", TokenType::AND},
    {"or", TokenType::OR},
    {"not", TokenType::NOT},
    {"while", TokenType::WHILE},
    {"do", TokenType::DO},
    {"let", TokenType::LET},
    {"ptr", TokenType::PTR},
    
    {"return", TokenType::RETURN},
    {"u1", TokenType::U1},
    {"u8", TokenType::U8},
    {"u16", TokenType::U16},
    {"u32", TokenType::U32},
    {"u64", TokenType::U64},
    {"i1", TokenType::I1},
    {"i8", TokenType::I8},
    {"i16", TokenType::I16},
    {"i32", TokenType::I32},
    {"i64", TokenType::I64},
    {"f32", TokenType::F32},
    {"f64", TokenType::F64},
    {"bool", TokenType::BOOL},
    {"char", TokenType::CHAR},
    {"string", TokenType::STRING},
    {"requires", TokenType::REQUIRES},
    {"ensures", TokenType::ENSURES},
    {"invariant", TokenType::INVARIANT},
    {"assert", TokenType::ASSERT},
    {"assume", TokenType::ASSUME},
    {"for", TokenType::FOR},
    {"break", TokenType::BREAK},
    {"continue", TokenType::CONTINUE},
    {"result", TokenType::RESULT},
    // NEW
    {"extern", TokenType::EXTERN},
    {"new", TokenType::NEW},
    {"delete", TokenType::DELETE_KW},
    {"true", TokenType::TRUE_KW},
    {"false", TokenType::FALSE_KW},
};

static bool isIdentStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

static bool isIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

Lexer::Lexer(std::string source)
    : source(std::move(source)) {}

char Lexer::peek() const {
    if (cursor >= source.length())
        return '\0';

    return source[cursor];
}

// NEW
char Lexer::peekNext() const {
    if (cursor + 1 >= source.length())
        return '\0';

    return source[cursor + 1];
}

char Lexer::advance() {
    if (cursor >= source.length())
        return '\0';

    char c = source[cursor++];

    if (c == '\n') {
        currentLine++;
        currentColumn = 1;
    } else {
        currentColumn++;
    }

    return c;
}

// CHANGED: also skips comments
void Lexer::skipWhitespace() {
    while (true) {
        char c = peek();

        if (std::isspace(static_cast<unsigned char>(c))) {
            advance();

        } else if (c == '/' && peekNext() == '/') {
            while (peek() != '\n' && peek() != '\0')
                advance();

        } else if (c == '/' && peekNext() == '*') {
            advance();
            advance();

            while (peek() != '\0' &&
                   !(peek() == '*' && peekNext() == '/')) {
                advance();
            }

            if (peek() != '\0') {
                advance();
                advance();
            }

        } else {
            break;
        }
    }
}

// NEW: called right after a backslash was consumed
char Lexer::readEscape() {
    char e = advance();

    switch (e) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case '0': return '\0';
        case '\\': return '\\';
        case '"': return '"';
        case '\'': return '\'';
        default: return e;
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (cursor < source.length()) {
        skipWhitespace();

        char current = peek();

        if (current == '\0')
            break;

        int tokenLine = currentLine;
        int tokenColumn = currentColumn;
        std::size_t tokenOffset = cursor;

        if (current == ';') {
            advance();

            tokens.push_back({
                TokenType::SEMICOLON, ";",
                tokenLine, tokenColumn, tokenOffset
            });

            continue;
        }

        // CHANGED: STRING_LITERAL + escapes + unterminated detection
        if (current == '"') {
            advance();

            std::string strVal;

            while (peek() != '"' && peek() != '\0') {
                char ch = advance();

                if (ch == '\\')
                    strVal += readEscape();
                else
                    strVal += ch;
            }

            if (peek() == '"') {
                advance();

                tokens.push_back({
                    TokenType::STRING_LITERAL, strVal,
                    tokenLine, tokenColumn, tokenOffset
                });
            } else {
                tokens.push_back({
                    TokenType::UNKNOWN, "unterminated string",
                    tokenLine, tokenColumn, tokenOffset
                });
            }

            continue;
        }

        // FIXED: char literal 'a' / '\n' + reject empty '' and multi-char 'ab'
        if (current == '\'') {
            advance(); // consume opening '\''

            if (peek() == '\'') {
                advance(); // consume closing '\''
                tokens.push_back({
                    TokenType::UNKNOWN, "empty char literal",
                    tokenLine, tokenColumn, tokenOffset
                });
                continue;
            }

            if (peek() == '\0') {
                tokens.push_back({
                    TokenType::UNKNOWN, "unterminated char literal",
                    tokenLine, tokenColumn, tokenOffset
                });
                continue;
            }

            std::string val;
            if (peek() == '\\') {
                advance(); // consume '\\'
                if (peek() == '\0') {
                    tokens.push_back({
                        TokenType::UNKNOWN, "unterminated char literal",
                        tokenLine, tokenColumn, tokenOffset
                    });
                    continue;
                }
                val += readEscape();
            } else {
                val += advance();
            }

            if (peek() == '\'') {
                advance(); // consume closing '\''
                tokens.push_back({
                    TokenType::CHAR_LITERAL, val,
                    tokenLine, tokenColumn, tokenOffset
                });
            } else if (peek() == '\0') {
                tokens.push_back({
                    TokenType::UNKNOWN, "unterminated char literal",
                    tokenLine, tokenColumn, tokenOffset
                });
            } else {
                // Consume extra characters up to closing '\'' or EOF so recovery doesn't cascade
                while (peek() != '\'' && peek() != '\0') {
                    advance();
                }
                if (peek() == '\'') {
                    advance();
                }
                tokens.push_back({
                    TokenType::UNKNOWN, "multi-character char literal",
                    tokenLine, tokenColumn, tokenOffset
                });
            }

            continue;
        }

        // CHANGED: float support (3.14)
        if (std::isdigit(
            static_cast<unsigned char>(current))) {

            std::string numVal;

            while (std::isdigit(
                static_cast<unsigned char>(peek()))) {
                numVal += advance();
            }

            bool isFloat = false;

            if (peek() == '.' &&
                std::isdigit(
                    static_cast<unsigned char>(peekNext()))) {

                isFloat = true;
                numVal += advance(); // '.'

                while (std::isdigit(
                    static_cast<unsigned char>(peek()))) {
                    numVal += advance();
                }
            }

            tokens.push_back({
                isFloat ? TokenType::FLOAT_LITERAL
                        : TokenType::NUMBER,
                numVal,
                tokenLine, tokenColumn, tokenOffset
            });

            continue;
        }

        // CHANGED: '_' allowed + keyword table
        if (isIdentStart(current)) {

            std::string ident;

            while (isIdentChar(peek())) {
                ident += advance();
            }

            if (ident == "else") {
                std::size_t savedCursor = cursor;
                int savedLine = currentLine;
                int savedColumn = currentColumn;

                skipWhitespace();

                bool isIf =
                    peek() == 'i' &&
                    peekNext() == 'f' &&
                    (cursor + 2 >= source.length() ||
                     !isIdentChar(source[cursor + 2]));

                if (isIf) {
                    advance();
                    advance();

                    tokens.push_back({
                        TokenType::ELSE_IF,
                        "else if",
                        tokenLine, tokenColumn, tokenOffset
                    });

                } else {
                    cursor = savedCursor;
                    currentLine = savedLine;
                    currentColumn = savedColumn;

                    tokens.push_back({
                        TokenType::ELSE, ident,
                        tokenLine, tokenColumn, tokenOffset
                    });
                }

            } else {
                auto it = keywords.find(ident);

                tokens.push_back({
                    it != keywords.end() ? it->second
                                         : TokenType::IDENTIFIER,
                    ident,
                    tokenLine, tokenColumn, tokenOffset
                });
            }

            continue;
        }

        // NEW: verify markers moved to #[ and ]#
        if (current == '#' && peekNext() == '[') {
            advance();
            advance();

            tokens.push_back({
                TokenType::VERIFY_START, "#[",
                tokenLine, tokenColumn, tokenOffset
            });

            continue;
        }

        if (current == ']' && peekNext() == '#') {
            advance();
            advance();

            tokens.push_back({
                TokenType::VERIFY_END, "]#",
                tokenLine, tokenColumn, tokenOffset
            });

            continue;
        }

        if (current == '=' && peekNext() == '=') {
            advance();
            advance();

            tokens.push_back({
                TokenType::EQUAL_EQUAL, "==",
                tokenLine, tokenColumn, tokenOffset
            });

            continue;
        }

        if (current == '!' && peekNext() == '=') {
            advance();
            advance();

            tokens.push_back({
                TokenType::NOT_EQUAL, "!=",
                tokenLine, tokenColumn, tokenOffset
            });

            continue;
        }

        if (current == '>' && peekNext() == '=') {
            advance();
            advance();

            tokens.push_back({
                TokenType::GREATER_EQUAL, ">=",
                tokenLine, tokenColumn, tokenOffset
            });

            continue;
        }

        if (current == '<' && peekNext() == '=') {
            advance();
            advance();

            tokens.push_back({
                TokenType::LESS_EQUAL, "<=",
                tokenLine, tokenColumn, tokenOffset
            });

            continue;
        }

        char c = advance();

        switch (c) {
            case '=':
                tokens.push_back({
                    TokenType::EQUAL, "=",
                    tokenLine, tokenColumn, tokenOffset
                });
                break;

            // NEW
            case '!':
                tokens.push_back({
                    TokenType::BANG, "!",
                    tokenLine, tokenColumn, tokenOffset
                });
                break;

            case '>':
                if (peek() == '>') {
                    advance();
                    tokens.push_back({
                        TokenType::SHIFT_RIGHT, ">>",
                        tokenLine, tokenColumn, tokenOffset
                    });
                } else {
                    tokens.push_back({
                        TokenType::GREATER, ">",
                        tokenLine, tokenColumn, tokenOffset
                    });
                }
                break;

            case '<':
                if (peek() == '<') {
                    advance();
                    tokens.push_back({
                        TokenType::SHIFT_LEFT, "<<",
                        tokenLine, tokenColumn, tokenOffset
                    });
                } else {
                    tokens.push_back({
                        TokenType::LESS, "<",
                        tokenLine, tokenColumn, tokenOffset
                    });
                }
                break;

            case '(':
                tokens.push_back({
                    TokenType::LPAREN, "(",
                    tokenLine, tokenColumn, tokenOffset
                });
                break;

            case ')':
                tokens.push_back({
                    TokenType::RPAREN, ")",
                    tokenLine, tokenColumn, tokenOffset
                });
                break;

            case ',':
                tokens.push_back({
                    TokenType::COMMA, ",",
                    tokenLine, tokenColumn, tokenOffset
                });
                break;

            case ':':
                tokens.push_back({
                    TokenType::COLON, ":",
                    tokenLine, tokenColumn, tokenOffset
                });
                break;

            // NEW
            case '.':
                tokens.push_back({
                    TokenType::DOT, ".",
                    tokenLine, tokenColumn, tokenOffset
                });
                break;

            // CHANGED: no more {{ -> plain braces only
            case '{':
                tokens.push_back({
                    TokenType::LBRACE, "{",
                    tokenLine, tokenColumn, tokenOffset
                });
                break;

            case '}':
                tokens.push_back({
                    TokenType::RBRACE, "}",
                    tokenLine, tokenColumn, tokenOffset
                });
                break;

            // NEW
            case '[':
                tokens.push_back({
                    TokenType::LBRACKET, "[",
                    tokenLine, tokenColumn, tokenOffset
                });
                break;

            case ']':
                tokens.push_back({
                    TokenType::RBRACKET, "]",
                    tokenLine, tokenColumn, tokenOffset
                });
                break;

            case '+':
                tokens.push_back({
                    TokenType::PLUS, "+",
                    tokenLine, tokenColumn, tokenOffset
                });
                break;

            case '-':
                if (peek() == '>') {
                    advance();
                    tokens.push_back({
                        TokenType::ARROW, "->",
                        tokenLine, tokenColumn, tokenOffset
                    });
                } else {
                    tokens.push_back({
                        TokenType::MINUS, "-",
                        tokenLine, tokenColumn, tokenOffset
                    });
                }
                break;

            case '*':
                tokens.push_back({
                    TokenType::MULTIPLY, "*",
                    tokenLine, tokenColumn, tokenOffset
                });
                break;

            case '/':
                tokens.push_back({
                    TokenType::DIVIDE, "/",
                    tokenLine, tokenColumn, tokenOffset
                });
                break;

            // NEW
            case '%':
                tokens.push_back({
                    TokenType::PERCENT, "%",
                    tokenLine, tokenColumn, tokenOffset
                });
                break;

            case '&':
                tokens.push_back({
                    TokenType::AMPERSAND, "&",
                    tokenLine, tokenColumn, tokenOffset
                });
                break;

            case '|':
                tokens.push_back({
                    TokenType::PIPE, "|",
                    tokenLine, tokenColumn, tokenOffset
                });
                break;

            case '^':
                tokens.push_back({
                    TokenType::CARET, "^",
                    tokenLine, tokenColumn, tokenOffset
                });
                break;

            default:
                tokens.push_back({
                    TokenType::UNKNOWN,
                    std::string(1, c),
                    tokenLine, tokenColumn, tokenOffset
                });
                break;
        }
    }

    tokens.push_back({
        TokenType::END_OF_FILE,
        "",
        currentLine,
        currentColumn,
        cursor
    });

    return tokens;
}
