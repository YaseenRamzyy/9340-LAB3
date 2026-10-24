#include "tokenizer.h"
#include <sstream>
#include <iostream>

// tokenize ligic is here and it returns a vector of tokens
std::vector<Token> tokenize(const std::string &input)
{
    std::vector<Token> tokens;
    std::istringstream stream(input);
    std::string word; 

    while (stream >> word) // keep reading while there is another words
    // >> auto separates the whitespaces
    {
        Token token;

        if (word == "|")
            token.type = TOKEN_PIPE;
        else if (word == ">")
            token.type = TOKEN_REDIRECT;
        else if (word == ">>")
            token.type = TOKEN_APPEND;
        else if (word == "<")
            token.type = TOKEN_INPUT;
        else if (word == "&")
            token.type = TOKEN_BACKGROUND;
        else if (word == "2>")
            token.type = TOKEN_ERROR;
        else if (word == ">>&")
            token.type = TOKEN_REDIRECT_AND_ERROR;
        else
            {
                if (!tokens.empty()) // if not a special symbol
                {
                    Token &prev = tokens.back();
                    if (prev.type == TOKEN_REDIRECT ||
                        prev.type == TOKEN_APPEND ||
                        prev.type == TOKEN_INPUT ||
                        prev.type == TOKEN_ERROR || 
                        prev.type == TOKEN_REDIRECT_AND_ERROR ||
                        prev.type == TOKEN_COMMAND || 
                        prev.type == TOKEN_ARGUMENT || 
                        word[0] == '-' ||
                        word.find('/') != std::string::npos ||
                        word.find('.') != std::string::npos

                    )
                    {
                        token.type = TOKEN_ARGUMENT;
                    }
                    else
                    {
                        token.type = TOKEN_COMMAND;
                    }
                }
                else
                {
                    token.type = TOKEN_COMMAND;
                }
            }

        token.value = word;
        tokens.push_back(token);
    }

    tokens.push_back({TOKEN_EOF, ""}); // add EOF token
    return tokens;
}
