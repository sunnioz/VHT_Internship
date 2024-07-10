#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/time.h>

#define PORT 8080
#define MAXLINE 1024

struct MIB {
    char message_id;
    short sfn_value;
};

volatile short gNodeB_sfn = 0;

void increment_sfn(int signum) {
    gNodeB_sfn = (gNodeB_sfn + 1) % 1024;
}

void setup_timer() {
    struct sigaction sa;
    struct itimerval timer;

    // Set up the signal handler
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &increment_sfn;
    sigaction(SIGALRM, &sa, NULL);

    // Configure the timer to expire after 10ms
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 10000;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 10000;

    // Start the timer
    setitimer(ITIMER_REAL, &timer, NULL);
}

int main() {
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    unsigned char buffer[MAXLINE];
    struct MIB mib;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    socklen_t len = sizeof(cliaddr);

    // Receive the address of UE from the initial request
    int n = recvfrom(sockfd, buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
    if (n > 0) {
        printf("Received request from UE.\n");
    }

    // Set up the timer for SFN increment
    setup_timer();

    while (1) {
        pause(); // Wait for the signal to increment SFN
        if (gNodeB_sfn % 8 == 0) { // 80ms delay
            mib.message_id = 1;
            mib.sfn_value = gNodeB_sfn;
            memcpy(buffer, &mib, sizeof(mib));

            sendto(sockfd, buffer, sizeof(mib), MSG_CONFIRM, (const struct sockaddr *)&cliaddr, len);
            printf("MIB message sent: SFN = %d\n", gNodeB_sfn);
        }
    }

    close(sockfd);
    return 0;
}
