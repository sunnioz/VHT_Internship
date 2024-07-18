#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<signal.h>
#include<sys/time.h>
#include<pthread.h>

#define UE_UDP_PORT 5000
#define AMF_TCP_PORT 6000

#define MAXLINE 1024
#define BUF_SIZE 1024


struct NgAP_Paging_message {
    int Message_Type;
    int UE_ID;
    int TAC;
    int CN_Domain;
};

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


bool flag_NgAP = false;


volatile short gNodeB_sfn = 0;

void increment_sfn(int signum) {
    gNodeB_sfn = (gNodeB_sfn + 1) % 1024;
}

// write a function to setup the timer
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


void *udp_server(void *arg){
    int udp_sockfd;
    struct sockaddr_in servaddr, cliaddr;
    char buffer[MAXLINE];
    struct MIB mib;

    if((udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(UE_UDP_PORT);

    if(bind(udp_sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    socklen_t len = sizeof(cliaddr);
    int n = recvfrom(udp_sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
    if(n > 0){
        printf("Receive message from UE:\n");
    }
    setup_timer();
    bool flag = true;
    while (1) {
        if (gNodeB_sfn % 8 == 0 && flag == true) { // 80ms delay
            flag = false;
            mib.message_id = 1;
            mib.sfn_value = gNodeB_sfn;
            memcpy(buffer, &mib, sizeof(mib));
            sendto(udp_sockfd, buffer, sizeof(mib), MSG_CONFIRM, (const struct sockaddr *)&cliaddr, len);
            //printf("MIB message sent: SFN = %d\n", gNodeB_sfn);
        }
        if (gNodeB_sfn % 8 != 0) {
            flag = true;
        }
        if(flag_NgAP && gNodeB_sfn % 80 == 0) {
            flag_NgAP = false;
            struct RRC_Paging_message rrc_paging_message;
            rrc_paging_message.Message_Type = 100;
            rrc_paging_message.UE_ID = 1234;
            rrc_paging_message.TAC = 100;
            rrc_paging_message.CN_Domain = 101;
            memcpy(buffer, &rrc_paging_message, sizeof(rrc_paging_message));
            sendto(udp_sockfd, buffer, sizeof(rrc_paging_message), MSG_CONFIRM, (const struct sockaddr *)&cliaddr, len);
            printf("Sent paging message to UE: UE_ID = %d, TAC = %d, CN_Domain = %d\n", rrc_paging_message.UE_ID, rrc_paging_message.TAC, rrc_paging_message.CN_Domain);
        
        }
    }
    return NULL;
}


void *tcp_server(void *arg) {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t add_size;
    char buffer[BUF_SIZE];

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(server_sock < 0){
        printf("socket error\n");
        exit(1);
    }
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(AMF_TCP_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        printf("bind error\n");
        exit(1);
    }

    listen(server_sock, 5);
    add_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &add_size);
    printf("AMF connected\n");
    bzero(buffer, BUF_SIZE);
    while (1)
    {
        int n = recv(client_sock, buffer, sizeof(buffer), 0);
        if (n > 0) {
            struct NgAP_Paging_message paging_message;
            memcpy(&paging_message, buffer, sizeof(paging_message));
            printf("Received paging message from AMF: UE_ID = %d, TAC = %d, CN_Domain = %d\n", paging_message.UE_ID, paging_message.TAC, paging_message.CN_Domain);
            flag_NgAP = true;
        }
        
    }
    
    return NULL;
}

int main(){
    pthread_t udp_thread, tcp_thread;
    pthread_create(&udp_thread, NULL, udp_server, NULL);
    pthread_create(&tcp_thread, NULL, tcp_server, NULL);

    pthread_join(udp_thread, NULL);
    pthread_join(tcp_thread, NULL);

    return 0;
}
