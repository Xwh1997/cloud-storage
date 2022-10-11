#include "Hash.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

Hash::Hash(string filename)
:_filename(filename)
{}

string Hash::sha1()
{
    int fd = open(_filename.c_str(), O_RDONLY);
    char buf[4096];
    //Init
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    //Update
    while(true){
        bzero(buf, sizeof(buf));
        int ret = read(fd, buf, sizeof(buf));
        if(ret == 0){
            break;
        }
        SHA1_Update(&ctx, buf, ret);
    }
    //Finally
    unsigned char md[20];
    SHA1_Final(md, &ctx);
    char flag[3];
    string sha1Res;
    for(int i = 0; i < 20; ++i){
        sprintf(flag, "%02x", md[i]);
        sha1Res += flag;
    }
    return sha1Res;
}
