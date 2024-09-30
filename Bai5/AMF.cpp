#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<signal.h>
#include<sys/time.h>
#include <time.h>

#define GNB_TCP_PORT 7000
#define MAXLINE 2048

const char* ipaddr = "127.0.0.1";

struct NgAP_Paging_message
{
    int Message_Type;
    int NG_5G_S_TMSI;
    int TAI;
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
    //addr.sin_addr.s_addr = INADDR_ANY;
    inet_pton(AF_INET,ipaddr,&addr.sin_addr);
    if(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1){
        printf("connect error\n");
        exit(1);
    }


    printf("Connected\n");

    /*
     *send NgAP paging message to gNodeB voi tan suat 5ms
     */
    long long cnt = 0;
    while(cnt < 10000){
        cnt ++;
        struct NgAP_Paging_message paging_message;
        paging_message.Message_Type = 100;
        paging_message.NG_5G_S_TMSI =100 + rand() % 200;
        paging_message.TAI = 8888;
        paging_message.CN_Domain = 101;
        memcpy(buffer, &paging_message, sizeof(paging_message));
        send(sock, buffer, sizeof(buffer), 0);
        printf("%lld - NgAP sent: NG_5G_S_TMSI = %d, TAI = %d, CN_Domain = %d\n", cnt, paging_message.NG_5G_S_TMSI, paging_message.TAI, paging_message.CN_Domain);
        usleep(100);
    }

    return 0;
}