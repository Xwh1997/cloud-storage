#define OPENSSL_API_COMPAT 0x10100000L
#include "Token.h"
#include <openssl/md5.h>

Token::Token(string username, string salt)
:_username(username)
,_salt(salt)
{}

string Token::getToken()
{
    //提取时间信息
    char timeStamp[20] = {0};
    time_t now = time(nullptr);
    struct tm *ptm = localtime(&now);
    sprintf(timeStamp, "%02d%02d%02d%02d", ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min);
    //MD5
    string toGen = _username + _salt;
    unsigned char md[16];
    MD5((unsigned char *)toGen.c_str(), toGen.size(), md);
    string res;
    char frag[3];
    for(int i = 0; i < 16; ++i){
        sprintf(frag, "%02x", md[i]);
        res += frag;
    }
    res += timeStamp;
    return res;
}