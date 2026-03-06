#ifndef PROTOCOL_H
#define PROTOCOL_H

#define BUFFER_SIZE 256
#define MAX_CAR_NAME 50
#define DEFAULT_PORT 7777

/* Команды клиента */
#define REQ_BUY "BUY"
#define REQ_SELL "SELL"
#define REQ_CARS "CARS"
#define REQ_EXIT "\\-"

/* Префиксы ответов */
#define PREFIX_INFO "INFO:"
#define PREFIX_OK "OK:"
#define PREFIX_ERR "ERR:"
#define PREFIX_ALERT "ALERT:"

#endif
