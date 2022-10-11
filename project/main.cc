#include "Hash.h"
#include "Crypt.h"
#include "Token.h"
#include "OSSInfo.h"
#include <workflow/WFFacilities.h>
#include <workflow/MySQLResult.h>
#include <workflow/MySQLUtil.h>
#include <wfrest/HttpServer.h>
#include <nlohmann/json.hpp>
#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <map>
#include <vector>
#include <iostream>

using namespace AlibabaCloud::OSS;
using Json = nlohmann::json;
using namespace AmqpClient;
using std::cout;
using std::endl;
using std::pair;
using std::string;
using std::map;
using std::vector;
static WFFacilities::WaitGroup waitGroup(1);

void sighandler(int signum)
{
    waitGroup.done();
}

int main()
{
    signal(SIGINT, sighandler);
    //创建服务端对象
    wfrest::HttpServer server;
    InitializeSdk();
    MQConfig mqconfig;
    //下载上传页面
    server.GET("/file/upload", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp){
        resp->File("static/view/index.html");
    });
    //上传接口
    server.POST("/file/upload", [&mqconfig](const wfrest::HttpReq *req, wfrest::HttpResp * resp, SeriesWork *series){
        //解析uri中的查询参数
        auto userInfo = req->query_list();
        string username = userInfo["username"];
        //保存文件
        using Form = map<string, pair<string, string>>;
        Form formData = req->form();    //解析form-data的报文请求
         for(auto &kv:formData){
             fprintf(stderr,"first = %s, second.first = %s, second.second = %s\n",
                            kv.first.c_str(),kv.second.first.c_str(),kv.second.second.c_str());
         }
        string filename = formData["file"].first;
        string filecontent = formData["file"].second;
        string filepath = "./tmp/" + filename;
        cout << "filename = " << filename << endl;
        int fd = open(filepath.c_str(), O_RDWR | O_CREAT, 0666);
        write(fd, filecontent.c_str(), filecontent.size());
        close(fd);

        Hash hash(filepath);
        string filehash = hash.sha1();
        string filesizeStr = std::to_string(filecontent.size());

        if(mqconfig.CurrentStoreType == storeType::OSS){
            if(mqconfig.isAsyncTransferEnable == false){
                //使用OSS备份，不使用异步转移
                //OSS客户端
                OSSInfo info;
                ClientConfiguration conf;
                OssClient client(info.EndPoint, info.AccessKeyID, info.AccessKeySecret, conf);
                string OSSPath = "oss/" + filehash;
                auto outcome = client.PutObject(info.Bucket, OSSPath, filepath);
                if(outcome.isSuccess() == false){
                fprintf(stderr, "Fail, code = %s, msg = %s, requestID = %s\n",
                    outcome.error().Code().c_str(),
                    outcome.error().Message().c_str(),
                    outcome.error().RequestId().c_str());
                    resp->set_status_code("500");
                    return;
                 }
            }else{
                //使用OSS备份，并且启用异步转移
                Channel::ptr_t channel = Channel::Create();
                //文件名字 文件hash值 文件的路径
                Json fileJson;
                fileJson["filehash"] = filehash;
                fileJson["filename"] = filename;
                fileJson["filepath"] = filepath;
                BasicMessage::ptr_t message = BasicMessage::Create(fileJson.dump());
                channel->BasicPublish(mqconfig.transExchange, mqconfig.transRoutingKey, message);
            }
        }
        
        //创建mysql任务，将信息存入数据库
        auto mysqlTask = WFTaskFactory::create_mysql_task("mysql://root:abcd@localhost", 0, [resp](WFMySQLTask *mysqlTask){
            if(mysqlTask->get_state() != WFT_STATE_SUCCESS){
                fprintf(stderr, "error msg:%s\n", WFGlobal::get_error_string(mysqlTask->get_state(), mysqlTask->get_error()));
                resp->set_status_code("500");   //服务器错误，万能报错
                return;
            }
            //从mysql中获取语法错误
            auto respMysql = mysqlTask->get_resp();
            if(respMysql->get_packet_type() == MYSQL_PACKET_ERROR){
                fprintf(stderr, "error code: %d, msg: %s\n", respMysql->get_error_code(), respMysql->get_error_msg().c_str());
                resp->set_status_code("500");
                return;
            }
            protocol::MySQLResultCursor cursor(respMysql);
            if(cursor.get_affected_rows() == 1){
                //重定向
                resp->set_status_code("302");
                resp->headers["Location"] = "/file/upload/success";
                return;
            }else{
                resp->set_status_code("500");
                return;
            }
        });
        string sql = "insert into HttpServer.tbl_file (file_sha1,file_name,file_size,file_addr,status) values(";
        sql += "'" + filehash + "','" + filename + "','" + filesizeStr + "','" + filepath + "',0);";
        sql += "insert into HttpServer.tbl_user_file (user_name,file_sha1,file_name,file_size,status) values('"
               + username + "','" + filehash + "','" + filename + "'," +filesizeStr + ",0);";
        //fprintf(stderr, "sql = %s\n", sql.c_str());
        //设置任务属性
        mysqlTask->get_req()->set_query(sql);
        //将任务放入序列中 
        series->push_back(mysqlTask);
    });
    //上传成功页面
    server.GET("/file/upload/success",[](const wfrest::HttpReq *req, wfrest::HttpResp *resp){
        resp->String("upload success!");
    });
    //下载接口
    server.GET("/file/download", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp){
        auto fileInfo = req->query_list();
        string filename = fileInfo["filename"];
        resp->headers["Location"] = "http://43.143.13.21:1235/" + filename;
        resp->set_status_code("302");
    });
    //获取注册页面
    server.GET("/user/signup", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp){
        resp->File("static/view/signup.html");
    });
    //注册接口
    server.POST("/user/signup", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp, SeriesWork *series){
        //获取账号密码
        auto formMap = req->form_kv();
        string username = formMap["username"];
        string password = formMap["password"];
        //加密密码
        string salt = "12345678";
        Crypt crypt(password, salt);
        //创建mysql任务将注册的用户信息更新进入数据库
        auto mysqlTask = WFTaskFactory::create_mysql_task("mysql://root:abcd@localhost", 0, [resp](WFMySQLTask *mysqlTask){
            //查看连接或者权限错误
            if(mysqlTask->get_state() != WFT_STATE_SUCCESS){
                fprintf(stderr, "error msg:%s\n", WFGlobal::get_error_string(resp->get_state(), resp->get_error()));
                resp->set_status_code("500");
                return;
            }
            //获取数据库语法错误
            auto respMySQL = mysqlTask->get_resp();
            if(respMySQL->get_packet_type() == MYSQL_PACKET_ERROR){
                fprintf(stderr, "error code: %d, msg: %s\n", respMySQL->get_error_code(), respMySQL->get_error_msg().c_str());
                resp->String("FAIL");
                return;
            }
            protocol::MySQLResultCursor cursor(respMySQL);
            if(cursor.get_affected_rows() == 1){
                resp->String("SUCCESS");
                return;
            }else{
                resp->String("FAIL");
                return;
            }
        });
        //设置任务属性
        string sql = "insert into HttpServer.tbl_user (user_name,user_pwd,status) values('"
                     + username + "','" + crypt.encoded() + "',0);";
        //fprintf(stderr, "sql = %s\n", sql.c_str());
        mysqlTask->get_req()->set_query(sql);
        series->push_back(mysqlTask);
    });
    //获取登录页面
    server.GET("/static/view/signin.html", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp){
        resp->File("static/view/signin.html");
    });
    //登录接口
    server.POST("/user/signin", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp, SeriesWork *series){
        //解析用户请求
        auto formInfo = req->form_kv();
        string username = formInfo["username"];
        string password = formInfo["password"];
        string salt = "12345678";

        auto mysqlTask = WFTaskFactory::create_mysql_task("mysql://root:abcd@localhost", 0, 
        [username, password, salt, resp](WFMySQLTask *mysqlTask){
            auto respMySQL = mysqlTask->get_resp();
            protocol::MySQLResultCursor cursor(respMySQL);
            vector<vector<protocol::MySQLCell>> rows;
            cursor.fetch_all(rows);
            if(rows.size() == 0){
                resp->set_status_code("500");
                return;
            }
            if(rows[0][0].is_string()){
                string resultMySQL = rows[0][0].as_string();
                Crypt crypt(password, salt);
                if(crypt.encoded() == resultMySQL){
                    //登录成功
                    Token token(username, "abcdefgh");
                    auto nextTask = WFTaskFactory::create_mysql_task("mysql://root:abcd@localhost", 0, nullptr);
                    //设置任务属性
                    string sql = "replace into HttpServer.tbl_user_token (user_name,user_token) values('"
                                 + username + "','" + token.getToken() + "');";
                    //fprintf(stderr, "sql = %s\n", sql.c_str());
                    nextTask->get_req()->set_query(sql);
                    series_of(mysqlTask)->push_back(nextTask);

                    Json respMsg;
                    Json data;
                    data["Token"] = token.getToken();
                    data["Username"] = username;
                    data["Location"] = "/static/view/home.html";
                    respMsg["data"] = data;
                    respMsg["code"] = 0;
                    respMsg["msg"] = "OK";
                    resp->String(respMsg.dump());
                    return;
                }else{  
                    resp->set_status_code("500");
                    return;
                }
            }else{
                resp->set_status_code("500");
                return;
            }
        });
        string sql = "select user_pwd from HttpServer.tbl_user where user_name ='" + username + "' LIMIT 1;";
        //fprintf(stderr, "sql = %s\n", sql.c_str());
        mysqlTask->get_req()->set_query(sql);
        series->push_back(mysqlTask);
    });
    //部署静态资源
    server.GET("/static/view/home.html", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp){
        resp->File("static/view/home.html");
    });
    server.GET("/static/js/auth.js", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp){
        resp->File("static/js/auth.js");
    });
    server.GET("/static/img/avatar.jpeg", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp){
        resp->File("static/img/avatar.jpeg");
    });
    //用户界面
    server.POST("/user/info", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp, SeriesWork *series){
        //解析请求
        auto userInfo = req->query_list();
        string username = userInfo["username"];
        cout << username << endl;
        //校验token，查询用户信息
        auto mysqlTask = WFTaskFactory::create_mysql_task("mysql://root:abcd@localhost", 0, [resp](WFMySQLTask *mysqlTask){
            auto respMySQL = mysqlTask->get_resp();
            protocol::MySQLResultCursor cursor(respMySQL);
            vector<vector<protocol::MySQLCell>> rows;
            cursor.fetch_all(rows);

            Json respMsg;
            Json data;
            data["Username"] = rows[0][0].as_string();
            data["SignupAt"] = rows[0][1].as_datetime();
            respMsg["data"] = data;
            respMsg["code"] = 0;
            respMsg["msg"] = "OK";
            resp->String(respMsg.dump());
        });
        string sql = "select user_name,signup_at from HttpServer.tbl_user where user_name='" + username +"' LIMIT 1;";
        //fprintf(stderr, "sql = %s\n", sql.c_str());
        mysqlTask->get_req()->set_query(sql);
        series->push_back(mysqlTask);
    });
    //更新用户文件列表
    server.POST("/file/query", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp, SeriesWork *series){
        //解析请求
        auto userInfo = req->query_list();
        string username = userInfo["username"];
        //校验token，解析报文体
        auto formKV = req->form_kv();
        string limit =formKV["limit"];
        auto mysqlTask = WFTaskFactory::create_mysql_task("mysql://root:abcd@localhost", 0, [resp](WFMySQLTask *mysqlTask){
            auto respMySQL = mysqlTask->get_resp();
            protocol::MySQLResultCursor cursor(respMySQL);
            vector<vector<protocol::MySQLCell>> rows;
            cursor.fetch_all(rows);

            Json arr;
            for(int i = 0; i < rows.size(); ++i){
                Json data;
                data["FileHash"] = rows[i][0].as_string();
                data["FileName"] = rows[i][1].as_string();
                data["FileSize"] = rows[i][2].as_ulonglong();
                data["UploadAt"] = rows[i][3].as_datetime();
                data["LastUpdated"] = rows[i][4].as_datetime();
                arr.push_back(data);
            }
            resp->String(arr.dump());
        });
        string sql = "select file_sha1,file_name,file_size,upload_at,last_update from HttpServer.tbl_user_file where user_name='"
                     + username +"' LIMIT " + limit + ";";
        mysqlTask->get_req()->set_query(sql);
        series->push_back(mysqlTask);
    });
    //下载链接
    server.POST("/file/downloadurl", [](const wfrest::HttpReq *req, wfrest::HttpResp *resp){
        auto fileInfo = req->query_list();
        string filename = fileInfo["filename"];
        string filehash = fileInfo["filehash"];
        //通过本地nginx服务器实现下载与业务分离
        //resp->String("http://43.143.13.21:1235/" + filename);
        //通过OSS对象存储
        ClientConfiguration conf;
        OSSInfo info;
        OssClient client(info.EndPoint, info.AccessKeyID, info.AccessKeySecret, conf);
        //上传文件请求
        string objectPath = "oss/" + filehash;
        time_t expire = time(nullptr) + 1200;
        auto outcome = client.GeneratePresignedUrl(info.Bucket, objectPath, expire, Http::Get);
        if(outcome.isSuccess() == false){
            fprintf(stderr, "Fail, code = %s, msg = %s, requestID = %s\n",
                    outcome.error().Code().c_str(),
                    outcome.error().Message().c_str(),
                    outcome.error().RequestId().c_str());
            resp->set_status_code("500");
        }else{
            fprintf(stderr,"Url = %s\n",outcome.result().c_str());
            resp->String(outcome.result());
        }
    });
    if(server.track().start(1234) == 0){
        server.list_routes();
        waitGroup.wait();
        server.stop();
        ShutdownSdk();
    }else{
        perror("Cannot start server!");
        return -1;
    }
    return 0;
}

