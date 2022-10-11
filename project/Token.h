#ifndef __TOKEN_H__
#define __TOKEN_H__
#include <string>
using std::string;

class Token
{
public:
    Token(string username, string salt);
    string getToken();
private:
    string _username;
    string _salt;
};
#endif