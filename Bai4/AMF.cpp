#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<signal.h>
#include<sys/time.h>

#define GNB_TCP_PORT 6000
#define MAXLINE 1024

struct NgAP_Paging_message {
    int Message_Type;
    int UE_ID;
    int TAC;
    int CN_Domain;
};

int main(){
    int sock;
    struct sockaddr_in addr;
    socklen_t add_size;
    char buffer[MAXLINE];
    int n;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock== -1){
        printf("socket error\n");
        exit(1);
    }
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(GNB_TCP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1){
        printf("connect error\n");
        exit(1);
    }


    printf("Connected\n");

    while(1){
        struct NgAP_Paging_message paging_message;
        paging_message.Message_Type = 100;
        paging_message.UE_ID = 1234;
        paging_message.TAC = 100;
        paging_message.CN_Domain = 101;

        memcpy(buffer, &paging_message, sizeof(paging_message));
        send(sock, buffer, sizeof(buffer), 0);
        printf("Sent\n");
        sleep(10);
    }

    return 0;
}