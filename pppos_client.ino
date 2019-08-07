#include "gsm.h"
#include "lwip/dns.h"
#include "lwip/inet.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

String input = "";
String output = "";
char c;

#define WEB_SERVER "www.w3.org"
#define WEB_URL "https://www.w3.org/TR/PNG/iso_8859-1.txt"
static const char *REQUEST = "GET " WEB_URL "\ HTTP/1.1\r\n"
    "Host: "WEB_SERVER"\r\n"
    "User-Agent: esp/1.0 esp32\r\n"
    "\r\n";

#define BUF_SIZE (1024)
char *data = (char *) malloc(BUF_SIZE);

struct in_addr getIp(char* hostname){
   ip_addr_t dnssrv=dns_getserver(0);
   Serial.print("DNS server: ");
   Serial.println(inet_ntoa(dnssrv));
   struct in_addr retAddr;
   struct hostent* he = gethostbyname(hostname);
   if (he == nullptr) {
      retAddr.s_addr = 0;
      Serial.println("Unable");
   } else {
      retAddr = *(struct in_addr*) (he->h_addr_list[0]);
      Serial.print("DNS get: ");
      Serial.println(inet_ntoa(retAddr));
   }
   return retAddr;
}

void getFile(struct in_addr retAddr, const char* request){
  int s, r;
  char recv_buf[5000];
  struct sockaddr_in sa;
  inet_aton(inet_ntoa(retAddr), &sa.sin_addr.s_addr);
  sa.sin_family = AF_INET;
  sa.sin_port = htons(80);
  s = socket(AF_INET, SOCK_STREAM, 0);
  if(s < 0) {
      Serial.println("Failed to allocate socket.");
  }
  if(connect(s, (struct sockaddr*)&sa, sizeof(sa)) != 0) {
      close(s);
      Serial.println("Failed to connect socket.");
  }
  Serial.println("connected.");
  if (write(s, request, strlen(request)) < 0) {
      Serial.println("Failed to send data.");
      close(s);
  }
  Serial.println("socket send success.");
  do {
      bzero(recv_buf, sizeof(recv_buf));
      r = read(s, recv_buf, sizeof(recv_buf)-1);
      Serial.write(recv_buf);
  }while(r > 0);
  close(s);
  Serial.println("socket closed.");
}

void setup()
{
  Serial.begin(115200);
  gsmInit(17, 16, 9600, 1);
  ppposInit("", "");
  Serial.println("1) Start GSM Communication: AT\\n");
  Serial.println("2) Configure GSM APN: AT+CGDCONT=1,\"IP\",\"internet\"\\n");
  Serial.println("3) Set GSM to PPP Mode: ATD*99***1#\\n or AT+CGDATA=\"PPP\",1\\n");
  Serial.println("4) Set ESP32 PPP: ppp\\n");
  Serial.println("5) Check if PPP works: get\\n");
  Serial.println("6) Try to download file: getfile\\n");
  Serial.println("7) Stop PPP Mode and return to AT command: stop\\n");
  Serial.println();
}

void loop(){
  if (Serial.available()){   
    c = (char)Serial.read();
    if (c == '\n'){
      if (input == "ppp"){
        Serial.println("ppp");
        ppposStart(); 
      } else if (input == "get"){
        getIp("google.com");
      } else if (input == "getfile"){
        getFile(getIp(WEB_SERVER), REQUEST);
      } else if (input == "stop"){
        Serial.println("stop");
        ppposStop();
      } else {
        Serial.println(input);
        input += "\n";
        gsmWrite((char *)input.c_str());
      }
      input = "";
    } else {
      input += c;
    }
  }

  if (!ppposStatus()){
    data = gsmRead();
    if (data != NULL){
      Serial.println(data);  
    }
  }
}

