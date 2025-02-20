// rvm assembler
// This is a one pass assembler

#include "rvm.hpp"
#include <string>
#include <cstddef>
#include <string_view>

enum class TokenKind {
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
            case '\n': {
                token.kind = TokenKind::NewLine;
                token.literal = ch;
                break;
            }

            default: {
                return read_identifier();
            }
        }

        read_ch();
        return token;
    }

    Token read_identifier() {}
};

int main() {}
