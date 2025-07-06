#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <ixwebsocket/IXHttp.h>
#include <ixwebsocket/IXWebSocketHttpHeaders.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketMessage.h>
#include <ixwebsocket/IXWebSocketMessageType.h>
#include <string>
#include <nlohmann/json.hpp>
#include <ixwebsocket/IXHttpClient.h>
#include <thread>
#include <chrono>
using nlohmann::json;


std::map<std::string,std::string> loadENV(const std::string& fileName){
  std::map<std::string,std::string> env;
  std::ifstream envFile(fileName);
  if (envFile.is_open()){
    std::string line;
    while(std::getline(envFile,line)){
       if (line.empty() || line[0] == '#') continue;
       size_t pos = line.find("=");
       if (pos == std::string::npos) continue;
       std::string key = line.substr(0,pos);
       std::string val = line.substr(pos+1);
       env[key]=val;
       setenv(key.c_str(), val.c_str(), 1);
    }
    envFile.close();
  } else std::cerr << "Failed to open ENV file." << std::endl;
  return env;
}


const char* BASE_URL = "https://discord.com/api/v10/";
const unsigned short INTENTS = 4608;
const std::string WEBKITFORMBOUNDARY = "--------WebKitFormBoundaryA3l5kj12LK4A68D59I0s";

std::string formFile(const std::string& fileName, const json& data){
  std::ifstream file(fileName, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string fileData = buffer.str();

  std::string body;
  body += "--" + WEBKITFORMBOUNDARY + "\r\n";
  body += "Content-Disposition: form-data; name=\"payload_json\"\r\n\r\n";
  body += R"({"type":4,"data":)"+data.dump()+R"(})";
  body += "\r\n--" + WEBKITFORMBOUNDARY + "\r\n";
  body += "Content-Disposition: form-data; name=\"files[0]\"; filename=\"ShutYo.mp4\"\r\n";
  body += "Content-Type: video/mp4\r\n\r\n";
  body += fileData;
  body += "\r\n--" + WEBKITFORMBOUNDARY + "--\r\n";

  return body;
}

void replaceAll(std::string& str, const std::string& toReplace, const std::string& substitution){
  if (toReplace.empty()) return;
  size_t pos = 0;
  while((pos = str.find(toReplace,pos)) != std::string::npos){
    str.replace(pos,toReplace.length(),substitution);
    pos += substitution.length();
  }
}

int main(){
  std::map<std::string,std::string> env = loadENV(".env");
  const std::string BOT_TOKEN = getenv("BOT_TOKEN");
  const std::string APPLICATION_ID = getenv("APPLICATION_ID");

  ix::HttpClient HttpClient;
  ix::HttpRequestArgsPtr args = HttpClient.createRequest();
  ix::WebSocketHttpHeaders headers;
  headers["Authorization"] = "Bot "+BOT_TOKEN;
  headers["Content-Type"] = "application/json";
  headers["User-Agent"] = "DiscordBot (TestBot, 1.0)";
  headers["Origin"] = "";

  args->extraHeaders = headers;

  HttpClient.post(std::string(BASE_URL)+"applications/"+APPLICATION_ID+"/commands",json({
        {"name","test"},
        {"description","Test command."},
        {"contexts",{0,1,2}}
        }).dump(),args);

  HttpClient.post(std::string(BASE_URL)+"applications/"+APPLICATION_ID+"/commands",json({
        {"name","urbansearch"},
        {"description","Search for a word on urban dictionary"},
        {"contexts",{0,1,2}},
        {"options",{
        {{"name","word"},
        {"description","The word you want to search for"},
        {"type",3}},
        }}
        }).dump(),args);


  ix::HttpResponsePtr out;
  out = HttpClient.get(std::string(BASE_URL)+"gateway",args);
  std::string WSSUrl = json::parse(out->body)["url"];
  std::cout << "Received WSS Url: " << WSSUrl << std::endl;

  ix::WebSocket WebSocket;
  WebSocket.setUrl(WSSUrl);

  WebSocket.setOnMessageCallback([&WebSocket,&HttpClient,args,BOT_TOKEN](const ix::WebSocketMessagePtr& msg){
      if (msg->type == ix::WebSocketMessageType::Message){
      //std::cout << msg->str << std::endl;
      json msgJSON = json::parse(msg->str);

      if (msgJSON["op"]==10){
      std::cout << "HELLO EVENT RECEIVED." << std::endl;
      unsigned short heartbeatInterval = msgJSON["d"]["heartbeat_interval"];
      std::cout << "HEARTBEAT INTERVAL: " << heartbeatInterval << std::endl;
      std::thread([&WebSocket,heartbeatInterval]{
          while(1){
          WebSocket.send(json({
                {"op",1},
                {"d", nullptr},
                }).dump());
          std::this_thread::sleep_for(std::chrono::milliseconds(heartbeatInterval));
          }
          }).detach();

      } else if (msgJSON["op"]==0){
      std::cout << "OPCODE IS 0" << std::endl;
      
      if (msgJSON["t"]=="INTERACTION_CREATE"){
        json interaction = msgJSON["d"];
        std::cout << "INTERACTION_CREATE RECEIVED" << std::endl;

        if (interaction["data"]["name"].get<std::string>() == "test"){

          std::cout << "TEST COMMAND RECEIVED" << std::endl;

          ix::HttpRequestArgsPtr args = HttpClient.createRequest();
          args->extraHeaders["Origin"] = "";
          args->extraHeaders["Content-Type"] = "multipart/form-data; boundary=" + WEBKITFORMBOUNDARY;

          std::string url = std::string(BASE_URL) + "interactions/" + interaction["id"].get<std::string>() + "/" + interaction["token"].get<std::string>() + "/callback";

          ix::HttpResponsePtr out;
          std::cout << "ID: " << interaction["id"].get<std::string>() << "    TOKEN: " << interaction["token"].get<std::string>() << std::endl;
          out = HttpClient.post(url,formFile("./assets/ShutYo.mp4", json({})),args);
          std::cout << "RESPONSE: " << out->body << std::endl;

        } else if (interaction["data"]["name"].get<std::string>() == "urbansearch"){

          ix::HttpRequestArgsPtr args = HttpClient.createRequest();
          args->extraHeaders["Origin"] = "";
          args->extraHeaders["Content-Type"] = "application/json";

          std::string url = std::string(BASE_URL) + "interactions/" + interaction["id"].get<std::string>() + "/" + interaction["token"].get<std::string>() + "/callback";

          if (interaction["data"]["options"].is_null() || interaction["data"]["options"].empty()){
            HttpClient.post(url,json({{"type",4},{"data",{{"content","You **_do_** need to specify the word you want to search for. You know that, right?"}}}}).dump(),args);
            return;
          } 

          std::string term = interaction["data"]["options"][0]["value"].get<std::string>();
          replaceAll(term," ","+");

          ix::HttpResponsePtr out = HttpClient.get("https://api.urbandictionary.com/v0/define?term="+term,args);
          if (out->statusCode != 200) {HttpClient.post(url,json({{"type",4},{"data",{{"content","Sending request to the UrbanDictionary API has failed."}}}}),args); return;}

          json response = json::parse(out->body)["list"];

          json fields = json::array();
          if (response.empty()){
            fields.push_back(json({{"name","No results"},{"value","We haven't any results for your search term"}}));
          } else {
            for (int i = 0; i<3; i++){
              if (response.size()-1<i) break;
              json result = response[i];
              std::string definition = result["definition"].get<std::string>();
              replaceAll(definition,"[","");replaceAll(definition, "]", "");
              fields.push_back(json({
                    {"name",result["word"]},
                    {"value","```\n"+(definition.length() > 1016 ? definition.substr(0, 1014)+".." : definition)+"\n```"}
                    }));
            }
          }

          std::cout << HttpClient.post(url,json({
                {"type",4},
                {"data",{
                {"content",""},
                {"embeds",{
                {
                {"title","Urban Dictionary"},
                {"description","Crowdsourced online dictionary of slang terms"},
                {"color",1135360},
                {"fields",fields},
                {"thumbnail",{{"url","https://www.urbandictionary.com/favicon-32x32.png"}}}
                }
                }}
                }}
                }).dump(),args)->body << std::endl;

        }

      } else if (msgJSON["t"]=="READY") {
        std::cout << "BOT IS NOW ONLINE" << std::endl;
      }
      }
      } else if (msg->type == ix::WebSocketMessageType::Open) {
        WebSocket.send(json({
              {"op",2},
              {"d",{
              {"token",BOT_TOKEN},
              {"properties",{
              {"os","linux"},
              {"browser","disco"},
              {"device","disco"}
              }},
              {"intents",INTENTS}
              }}
              }).dump()); 
        std::cout << "SENT IDENTIFY" << std::endl;

      } else if (msg->type == ix::WebSocketMessageType::Error) {
        std::cout << "Connection error: " << msg->errorInfo.reason << std::endl;
        std::cout << "> " << std::flush;
      }
  });

  WebSocket.start();
  std::cin.get();
  WebSocket.close();
  return 0;
}
