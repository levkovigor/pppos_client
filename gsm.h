#ifndef _GSM_H_
#define _GSM_H_
#ifdef __cplusplus
extern "C" {
#endif

void ppposInit(char* user, char* pass);

bool ppposConnectionStatus();

void ppposStart();

bool ppposStatus();

void ppposStop();

void gsmInit(int txPin, int rxPin, int baudrate, int uart_number);

void gsmWrite(char* cmd);

char* gsmRead();

#ifdef __cplusplus
}
#endif
#endif
