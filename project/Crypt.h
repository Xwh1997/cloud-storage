#ifndef __CRYPT_H__
#define __CRYPT_H__
#include <string>
using std::string;

class Crypt
{
public:
    Crypt(string key, string salt);
    string encoded();
private:
    string _key;
    string _salt;
};
#endif