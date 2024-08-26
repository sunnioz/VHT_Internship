#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>
#include <vector>

#define UE_UDP_PORT 5000
#define AMF_TCP_PORT 6000
#define MAXLINE 1024
#define BUF_SIZE 1024


#define N 4
#define DRX_cycle 128
#define PF_offset 0

pthread_mutex_t flag_mutex = PTHREAD_MUTEX_INITIALIZER;

struct UE_message
{
    int UE_ID;
    char *payload;
};

struct NgAP_Paging_message
{
    int Message_Type;
    int UE_ID;
    int TAC;
    int CN_Domain;
};

struct Paging_Record
{
    int UE_ID;
    int access_type;
};

struct RRC_Paging_message
{
    int Message_Type;
    int num_Paging_record;
    struct Paging_Record paging_record[64];
};

struct MIB
{
    char message_id;
    short sfn_value;
};

struct SIB1
{
    short s_PF_offset;
    short s_DRX_cycle;
    short s_N;
};

volatile short gNodeB_sfn = 0;
struct SIB1 sib1 = {PF_offset, DRX_cycle, N};
struct NgAP_Paging_message paging_message;

// moi ban tin NgAP_Paging_message se duoc phan vao 1 trong N nhom luu vao mang rrc_paging_message
struct RRC_Paging_message rrc_paging_message[N];
// moi client cung duoc phan vao nhom
struct sockaddr_in group_cliaddr[N];

void print_client_info(struct sockaddr_in *cliaddr)
{

    char *client_ip = inet_ntoa(cliaddr->sin_addr);
    int client_port = ntohs(cliaddr->sin_port);
    printf("Client IP: %s\n", client_ip);
    printf("Client Port: %d\n", client_port);
}

// ham khoi tao gia tri mac dinh cho rrc_paging_message
void init_rrc_paging_message()
{
    for (int i = 0; i < N; i++)
    {
        rrc_paging_message[i].Message_Type = 100;
        rrc_paging_message[i].num_Paging_record = 0;
    }
}
// ham reset cac gi tri cua rrc_paging_message thu i
void reset_rrc_paging_message(int i)
{
    memset(&rrc_paging_message[i], 0, sizeof(rrc_paging_message[i]));
    rrc_paging_message[i].Message_Type = 100;
    rrc_paging_message[i].num_Paging_record = 0;
}

void phan_nhom(struct NgAP_Paging_message paging_message)
{
    int group = paging_message.UE_ID % N;
    if (rrc_paging_message[group].num_Paging_record == 64)
    {
        reset_rrc_paging_message(group);
    }
    rrc_paging_message[group].paging_record[rrc_paging_message[group].num_Paging_record].UE_ID = paging_message.UE_ID;
    rrc_paging_message[group].paging_record[rrc_paging_message[group].num_Paging_record].access_type = paging_message.TAC;
    rrc_paging_message[group].num_Paging_record++;
    printf("Da phan nhom cho ban tin paging cho UE %d vao nhom %d\n", paging_message.UE_ID, group);
}

// ham kiem tra num_Paging_record cua moi nhom rrc_paging_message
bool check_Num_Paging_record(int i)
{
    if (rrc_paging_message[i].num_Paging_record == 0)
    {
        return false;
    }
    return true;
}
// ham xem nhung thu co trong rrc_paging_message
void in_rrc_paging_message()
{
    for (int i = 0; i < N; i++)
    {
        printf("RRC Paging Message %d:\n", i);
        printf("  Message_Type: %d\n", rrc_paging_message[i].Message_Type);
        printf("  num_Paging_record: %d\n", rrc_paging_message[i].num_Paging_record);

        for (int j = 0; j < rrc_paging_message[i].num_Paging_record; j++)
        {
            printf("    Paging Record %d:\n", j);
            printf("      UE_ID: %d\n", rrc_paging_message[i].paging_record[j].UE_ID);
            printf("      access_type: %d\n", rrc_paging_message[i].paging_record[j].access_type);
        }
        printf("\n");
    }
}



void increment_sfn(int signum)
{
    gNodeB_sfn = (gNodeB_sfn + 1) % 1024;
}

// write a function to setup the timer
void setup_timer()
{
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

// tinh toan cac PF dau tien cua moi chu ky DRX tuong ung voi nhom UE_ID
int PF_first_s[N];
// (SFN + PF_offset)%T = (T / N) * (UE_ID % N);
void cal_PF_first_s()
{
    for (int i = 0; i < N; i++)
    {
        PF_first_s[i] = (DRX_cycle / N) * i;
    }
}

void *udp_server(void *arg)
{
    int udp_sockfd;
    struct sockaddr_in servaddr, cliaddr;
    char buffer[MAXLINE];
    struct MIB mib;

    if ((udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(UE_UDP_PORT);

    if (bind(udp_sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    bool flag = false;
    while (1)
    {
        // bat ket noi UE
        socklen_t len = sizeof(cliaddr);
        int n = recvfrom(udp_sockfd, (char *)buffer, MAXLINE, MSG_DONTWAIT, (struct sockaddr *)&cliaddr, &len);

        if (n > 0)
        {
            struct UE_message ue_message;
            memcpy(&ue_message, buffer, sizeof(ue_message));
            printf("UE_ID_S received from UE: %d\n", ue_message.UE_ID);
            group_cliaddr[ue_message.UE_ID % N] = cliaddr;
            print_client_info(&group_cliaddr[ue_message.UE_ID % N]);
            memcpy(buffer, &sib1, sizeof(sib1));
            sendto(udp_sockfd, buffer, sizeof(sib1), MSG_CONFIRM, (const struct sockaddr *)&cliaddr, len);
            // printf("%d", buffer[0]);
            printf("SIB1 message sent to UE\n");
        }

        for (int i = 0; i < N; i++)
        {
            if(i == 1) cliaddr = group_cliaddr[1];
            if(i == 2) cliaddr = group_cliaddr[2];
            if(i == 3 || i == 4) continue;
            for (int j = PF_first_s[i]; j < 1024; j += DRX_cycle)
            {
                if (j == gNodeB_sfn)
                {
                    pthread_mutex_lock(&flag_mutex);

                    memcpy(buffer, &rrc_paging_message[i], sizeof(rrc_paging_message[i]));
                    if (check_Num_Paging_record(i) == true)
                    {
                        sendto(udp_sockfd, buffer, sizeof(rrc_paging_message[i]), MSG_CONFIRM, (const struct sockaddr *)&cliaddr, len);
                        printf("RRC Paging message sent to UE: SFN = %d\n", gNodeB_sfn);
                        reset_rrc_paging_message(i);
                    }

                    pthread_mutex_unlock(&flag_mutex);
                }
            }
        }

        if (gNodeB_sfn % 8)
        {
            flag = true;
        }
        if (gNodeB_sfn % 8 == 0 && flag) // 80ms delay
        {
            flag = false;
            mib.message_id = 1;
            mib.sfn_value = gNodeB_sfn;
            memcpy(buffer, &mib, sizeof(mib));
            sendto(udp_sockfd, buffer, sizeof(mib), MSG_CONFIRM, (const struct sockaddr *)&group_cliaddr[1], len);
            sendto(udp_sockfd, buffer, sizeof(mib), MSG_CONFIRM, (const struct sockaddr *)&group_cliaddr[2], len);

            // printf("MIB message sent: SFN = %d\n", gNodeB_sfn);
        }
    }
    return NULL;
}

void *tcp_server(void *arg)
{
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t add_size;
    char buffer[BUF_SIZE];

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0)
    {
        printf("socket error\n");
        exit(1);
    }
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(AMF_TCP_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("bind error\n");
        exit(1);
    }
    listen(server_sock, 5);
    add_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &add_size);
    printf("AMF connected\n");
    bzero(buffer, BUF_SIZE);
    while (1)
    {
        int n = recv(client_sock, buffer, sizeof(buffer), 0);
        if (n > 0)
        {
            memcpy(&paging_message, buffer, sizeof(paging_message));
            printf("gNodeB:%d,Received paging message from AMF: UE_ID = %d, TAC = %d, CN_Domain = %d\n", gNodeB_sfn, paging_message.UE_ID, paging_message.TAC, paging_message.CN_Domain);

            pthread_mutex_lock(&flag_mutex);
            phan_nhom(paging_message);
            pthread_mutex_unlock(&flag_mutex);
            // in_rrc_paging_message();
        }
    }

    return NULL;
}

int main()
{
    setup_timer();
    cal_PF_first_s();
    init_rrc_paging_message();

    pthread_t udp_thread, tcp_thread;
    pthread_create(&udp_thread, NULL, udp_server, NULL);
    pthread_create(&tcp_thread, NULL, tcp_server, NULL);

    pthread_join(udp_thread, NULL);
    pthread_join(tcp_thread, NULL);

    return 0;
}
