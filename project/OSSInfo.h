#include <alibabacloud/oss/OssClient.h>
#include <string>

using std::string;

struct OSSInfo{
    string Bucket = "xwh-bucket-server-aliyun";
    string EndPoint = "oss-cn-hangzhou.aliyuncs.com";
    string AccessKeyID = "LTAI5tHQuR8KXjTLuXjKta7v";
    string AccessKeySecret = "8WfC8If2IChs1u1nzvUO0odLJ86iT9";
};

enum storeType
{
    LOCAL,
    OSS
};

struct MQConfig
{
    // 是否开启备份
    enum storeType CurrentStoreType = storeType::OSS;
    // 是否启用异步转移
    bool isAsyncTransferEnable = true;
    // 交换器的名称
    string transExchange = "uploadserver.trans";
    // rountingkey
    string transRoutingKey = "oss";
};