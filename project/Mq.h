#include <string>
using std::string;

struct MQInfo
{
    string URL ="amqp://guest:guest@127.0.0.1:5672";
    string Exchange = "uploadserver.trans";
    string OSSQueue = "uploadserver.trans.oss";
    string RoutingKey = "oss";
};