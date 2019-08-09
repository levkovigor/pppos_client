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
#include "FS.h"
#include "SPIFFS.h"
#include <Update.h>

String input = "";
String output = "";
char c;

#define WEB_SERVER "factorial-group.com.ua"
#define WEB_URL "http://factorial-group.com.ua/upload.bin"
static const char *REQUEST = "GET " WEB_URL "\ HTTP/1.1\r\n"
                             "Host: "WEB_SERVER"\r\n"
                             "User-Agent: esp/1.0 esp32\r\n"
                             "\r\n";

#define BUF_SIZE (1024)
char *data = (char *) malloc(BUF_SIZE);

struct in_addr getIp(char* hostname) {
  ip_addr_t dnssrv = dns_getserver(0);
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

void getFile(struct in_addr retAddr, const char* request) {
  int s, r;
  char recv_buf[5000];
  struct sockaddr_in sa;
  inet_aton(inet_ntoa(retAddr), &sa.sin_addr.s_addr);
  sa.sin_family = AF_INET;
  sa.sin_port = htons(80);
  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    Serial.println("Failed to allocate socket.");
  }

  if (connect(s, (struct sockaddr*)&sa, sizeof(sa)) != 0) {
    close(s);
    Serial.println("Failed to connect socket.");
  }
  Serial.println("connected.");
  if (write(s, request, strlen(request)) < 0) {
    Serial.println("Failed to send data.");
    close(s);
  }
  Serial.println("socket send success.");
  bool headerFound = false;
  SPIFFS.remove("/update.bin");
  File file = SPIFFS.open("/update.bin", FILE_WRITE);
  file = SPIFFS.open("/update.bin", FILE_APPEND);
  int contentLength = 0;
  int currentLength = 0;
  do {
    bzero(recv_buf, sizeof(recv_buf));
    r = read(s, recv_buf, sizeof(recv_buf) - 1);    
    if (!headerFound) {

        if (contentLength == 0) {
                   char* cont_len = strstr(recv_buf, "Content-Length: ");
                    if (cont_len) {
                        cont_len += 16;
                        if (strstr(cont_len, "\r\n")) {
                            char slen[16] = {0};
                            memcpy(slen, cont_len, strstr(cont_len, "\r\n")-cont_len);
                            contentLength = atoi(slen);
                            Serial.printf("Content-Length: %d\n", contentLength);
                        }
                    }
        }
        
      
        char* srtr = strstr(recv_buf, "\r\n\r\n");
        if (srtr != NULL){
           int header = srtr - recv_buf;
           memmove(recv_buf,  srtr + 4, sizeof(recv_buf));
           file = SPIFFS.open("/update.bin", FILE_APPEND);
           file.write((const uint8_t*)recv_buf, r-header-4);
           currentLength +=  r-header-4;      
           headerFound = true;        
      } else {
        //Serial.write((const uint8_t*)recv_buf, r);
      }
    } else {
      file = SPIFFS.open("/update.bin", FILE_APPEND);
      file.write((const uint8_t*)recv_buf, r);
      currentLength +=  r;
      Serial.printf("%.2f percents\n", (float)currentLength/(float)contentLength*100.0);
    }
  } while (r > 0);
  close(s);
  Serial.println("socket closed.");
  updateFromFS(SPIFFS);
  ESP.restart();
}

#define FORMAT_SPIFFS_IF_FAILED true

void performUpdate(Stream &updateSource, size_t updateSize) {
  if (Update.begin(updateSize)) {
    size_t written = Update.writeStream(updateSource);
    if (written == updateSize) {
      Serial.println("Written : " + String(written) + " successfully");
    }
    else {
      Serial.println("Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
    }
    if (Update.end()) {
      Serial.println("OTA done!");
      if (Update.isFinished()) {
        Serial.println("Update successfully completed. Rebooting.");
      }
      else {
        Serial.println("Update not finished? Something went wrong!");
      }
    }
    else {
      Serial.println("Error Occurred. Error #: " + String(Update.getError()));
    }

  }
  else
  {
    Serial.println("Not enough space to begin OTA");
  }
}

// check given FS for valid update.bin and perform update if available
void updateFromFS(fs::FS &fs) {
  File updateBin = fs.open("/update.bin");
  if (updateBin) {
    if (updateBin.isDirectory()) {
      Serial.println("Error, update.bin is not a file");
      updateBin.close();
      return;
    }

    size_t updateSize = updateBin.size();

    if (updateSize > 0) {
      Serial.println("Try to start update");
      performUpdate(updateBin, updateSize);
    }
    else {
      Serial.println("Error, file is empty");
    }

    updateBin.close();

    // whe finished remove the binary from sd card to indicate end of the process
    fs.remove("/update.bin");
  }
  else {
    Serial.println("Could not load update.bin from sd root");
  }
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
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }

}

void loop() {
  if (Serial.available()) {
    c = (char)Serial.read();
    if (c == '\n') {
      if (input == "ppp") {
        Serial.println("ppp");
        ppposStart();
      } else if (input == "get") {
        getIp("google.com");
      } else if (input == "getfile") {
        getFile(getIp(WEB_SERVER), REQUEST);
      } else if (input == "stop") {
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

  if (!ppposStatus()) {
    data = gsmRead();
    if (data != NULL) {
      Serial.println(data);
    }
  }
}

