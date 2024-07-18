#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<signal.h>
#include<sys/time.h>

#define GNB_UDP_PORT 5000
#define MAXLINE 1024

struct RRC_Paging_message {
    int Message_Type;
    int UE_ID;
    int TAC;
    int CN_Domain;
};

struct MIB {
    char message_id;
    short sfn_value;
};

volatile short UE_sfn = 0; // Biến toàn cục để tăng SFN
int sync_status = 0; // 0: not synced, 1: synced

void increment_sfn(int signum) {
    UE_sfn = (UE_sfn + 1) % 1024;
}


void setup_timer() {
    struct sigaction sa;
    struct itimerval timer;

    // Thiết lập hàm xử lý tín hiệu
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &increment_sfn;
    sigaction(SIGALRM, &sa, NULL);


    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 10000;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 10000;

    setitimer(ITIMER_REAL, &timer, NULL);
}

int main(){
    int sockfd;
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in servaddr;
    char buffer[MAXLINE];
    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(GNB_UDP_PORT);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    socklen_t len = sizeof(servaddr);
    const char *message = "Hello from UE";
    sendto(sockfd, (const char *)message, strlen(message), MSG_CONFIRM, (const struct sockaddr *)&servaddr, len);
    printf("Message sent from UE\n");

    setup_timer();

    while(1){
        int n = recvfrom(sockfd, buffer, MAXLINE, MSG_DONTWAIT, (struct sockaddr *)&servaddr, &len);
        if (n > 0) {
            if(buffer[0] == 100){
                struct RRC_Paging_message paging_message;
                memcpy(&paging_message, buffer, sizeof(paging_message));
                printf("Received RRC Paging message: UE_ID = %d, TAC = %d, CN_Domain = %d\n", paging_message.UE_ID, paging_message.TAC, paging_message.CN_Domain);
            }
            else{
                struct MIB mib;
                memcpy(&mib, buffer, sizeof(mib));
                //printf("Received MIB: SFN = %d, UE_sfn = %d\n", mib.sfn_value, UE_sfn);
                if (sync_status == 0) {
                    UE_sfn = mib.sfn_value;
                    sync_status = 1;
                    printf("UE synced to SFN = %d\n", UE_sfn);
                } else if (mib.sfn_value % 80 == 0) { // cu moi 800ms thi UE re-sync
                    UE_sfn = mib.sfn_value;
                    printf("UE re-synced to SFN = %d\n", UE_sfn);
                }
            }
            

        }
    }

}