#include "Mq.h"
#include "unixHeader.h"
#include <SimpleAmqpClient/SimpleAmqpClient.h>
using namespace AmqpClient;

int main()
{
    MQInfo mqinfo;
    //创建一个channel，通信通道
    Channel::ptr_t channel = Channel::Create();
    pause();
    return 0;
}