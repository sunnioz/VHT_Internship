#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/time.h>

#define PORT 8080
#define UE_PORT 8081
#define MAXLINE 1024

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

int main() {
    int sockfd;
    struct sockaddr_in servaddr, ueaddr;
    unsigned char buffer[MAXLINE];

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&ueaddr, 0, sizeof(ueaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    ueaddr.sin_family = AF_INET;
    ueaddr.sin_port = htons(UE_PORT);
    ueaddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (const struct sockaddr *)&ueaddr, sizeof(ueaddr)) < 0) {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    socklen_t len = sizeof(servaddr);

    // Gửi yêu cầu ban đầu tới gNodeB để thông báo địa chỉ của UE
    const char *init_message = "UE requesting MIB";
    sendto(sockfd, init_message, strlen(init_message), MSG_CONFIRM, (const struct sockaddr *)&servaddr, len);
    printf("Sent to gNodeB.\n");

    setup_timer();

    while (1) {
        int n = recvfrom(sockfd, buffer, MAXLINE, MSG_DONTWAIT, (struct sockaddr *)&servaddr, &len);
        if (n > 0) {
            struct MIB mib;
            memcpy(&mib, buffer, sizeof(mib));
            printf("Received MIB: SFN = %d, UE_sfn = %d\n", mib.sfn_value, UE_sfn);
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

    close(sockfd);
    return 0;
}
