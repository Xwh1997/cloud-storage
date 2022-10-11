#include "Crypt.h"
#include <crypt.h>

Crypt::Crypt(string key, string salt)
:_key(key)
,_salt(salt)
{}

string Crypt::encoded()
{
    string res = crypt(_key.c_str(), _salt.c_str());
    return res;
}