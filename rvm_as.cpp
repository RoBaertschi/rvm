// rvm assembler
// This is a one pass assembler

#include "rvm.hpp"
#include <string>
#include <cstddef>
#include <string_view>

enum class TokenKind {
    Eof,
    Invalid,

    Identifier,
    Label,

    NewLine,

    Object,

    // Instructions
#define _X(kind, ...) kind,
    INSTRUCTION_KIND
#undef _X
};

struct Token {
    // Only valid for TokenKind::Object
    TokenKind   kind;
    std::string literal;
    size_t      object_id = 0;
};

bool is_number(char ch) {
    return '0' <= ch && ch <= '9';
}

bool is_valid_identifier(char ch) {
    return ch == '.'
        || ch == '_'
        || ('a' <= ch && ch <= 'z')
        || ('A' <= ch && ch <= 'Z');
}

struct Lexer {
    std::string_view    input;
    size_t              read_pos, pos;
    char                ch;

    void read_ch() {
        if (read_pos >= input.length()) {
            ch = 0;
        } else {
            ch = input[read_pos];
            pos = read_pos;
            read_pos += 1;
        }
    }

    char peek_char() {
        return input[read_pos];
    }

    Token next_token() {
        Token token{};

        switch (ch) {
            case 0: {
                token.kind = TokenKind::Eof;
                token.literal = "";
                break;
            }
            case '\n': {
                token.kind = TokenKind::NewLine;
                token.literal = ch;
                break;
            }

            default: {
                if (is_valid_identifier(ch)) {
                    return read_identifier();
                }
                token.kind = TokenKind::Invalid;
                token.literal = ch;
            }
        }

        read_ch();
        return token;
    }

    Token read_identifier() {}
};

int main() {}
