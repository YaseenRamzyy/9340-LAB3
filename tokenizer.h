#ifndef tokenizer_h // include header guard
#define tokenizer_h 

#include <string>
#include <vector>

// enum for all the token types
enum TokenType
{
    TOKEN_COMMAND,    
    TOKEN_ARGUMENT,   
    TOKEN_REDIRECT,  
    TOKEN_APPEND,   
    TOKEN_INPUT,     
    TOKEN_ERROR,    
    TOKEN_PIPE,       
    TOKEN_BACKGROUND, 
    TOKEN_REDIRECT_AND_ERROR,
    TOKEN_EOF,       
};

// each token has type and the actual wording
struct Token
{
    TokenType type;
    std::string value;
};

std::vector<Token> tokenize(const std::string &input);

#endif