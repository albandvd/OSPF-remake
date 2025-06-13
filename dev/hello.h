#ifndef HELLO_H
#define HELLO_H

#define MSG_TYPE_HELLO 1
#define MSG_TYPE_HELLO_ACK 2
#define MAX_NAME_LEN 32

typedef struct
{
    int type;
    int router_id;
    char router_name[MAX_NAME_LEN];
} HelloMessage;

typedef struct
{
    int type;
    int router_id;
    int status;
} HelloAckMessage;

#endif
