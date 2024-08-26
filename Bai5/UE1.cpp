#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/time.h>
#include <vector>

#define UE_ID_S 102
#define GNB_UDP_PORT 5000
#define MAXLINE 1024


struct UE_message
{
    int UE_ID;
    const char *payload;
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
    short PF_offset;
    short DRX_cycle;
    short N;
};

volatile short UE_sfn = 0;
int sync_status = 0;
struct UE_message ue_message = {UE_ID_S, "Hello from UE"};

void increment_sfn(int signum)
{
    UE_sfn = (UE_sfn + 1) % 1024;
}

// std ::vector<short> PFs;

// std ::vector<short> cal_PFs(short PF_offset, short DRX_cycle, short N, short UE_ID)
// {
//     short T = DRX_cycle;
//     short VP = (T / N) * (UE_ID % N);
//     short start = VP;
//     std ::vector<short> res;
//     for (short i = VP; i < 1024; i += T)
//     {
//         res.push_back(i);
//     }
//     return res;
// }

int PFs[8];
void cal_PFs(short PF_offset, short DRX_cycle, short N, short UE_ID)
{
    short T = DRX_cycle;
    short VP = (T / N) * (UE_ID % N);
    short start = VP;
    int cnt = 0;
    for (short i = VP; i < 1024; i += T)
    {
        PFs[cnt++] = i;
    }
 
}

void setup_timer()
{
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

int main()
{
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
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

    sendto(sockfd, &ue_message, sizeof(ue_message), MSG_CONFIRM, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    printf("Message with UE_ID_S sent from UE\n");

    setup_timer();

    while (1)
    {
        int n = recvfrom(sockfd, buffer, MAXLINE, MSG_DONTWAIT, (struct sockaddr *)&servaddr, &len);
        if (n > 0)
        {
            if (buffer[0] == 1)
            {
                struct MIB mib;
                memcpy(&mib, buffer, sizeof(mib));
                // printf("Received MIB: SFN = %d, UE_sfn = %d\n", mib.sfn_value, UE_sfn);
                if (sync_status == 0)
                {
                    UE_sfn = mib.sfn_value;
                    sync_status = 1;
                    printf("UE synced to SFN = %d\n", UE_sfn);
                }
                else if (mib.sfn_value % 80 == 0)
                { // cu moi 800ms thi UE re-sync
                    UE_sfn = mib.sfn_value;
                    printf("UE re-synced to SFN = %d\n", UE_sfn);
                }
            }
            else if (buffer[0] == 0)
            {
                struct SIB1 sib1;
                memcpy(&sib1, buffer, sizeof(sib1));
                printf("Received SIB1: PF_offset = %d, DRX_cycle = %d, N = %d\n", sib1.PF_offset, sib1.DRX_cycle, sib1.N);
                if (sib1.N == 0)
                    continue;
                cal_PFs(sib1.PF_offset, sib1.DRX_cycle, sib1.N, UE_ID_S);
                for (int i = 0; i < 8;i++)
                {
                    printf("PF = %d ", PFs[i]);
                }
            }
        }
        for (int i = 0; i < 8;i++)
        {
            if (UE_sfn == PFs[i])
            {
                if (n > 0)
                {
                    if (buffer[0] == 100)
                    {
                        struct RRC_Paging_message paging_message;
                        memcpy(&paging_message, buffer, sizeof(paging_message));

                        printf("Received RRC Paging Message:\n");
                        printf("  Message_Type: %d\n", paging_message.Message_Type);
                        printf("  Number of Paging Records: %d\n", paging_message.num_Paging_record);

                        for (int i = 0; i < paging_message.num_Paging_record; i++)
                        {
                            printf("  Paging Record %d:\n", i + 1);
                            printf("    UE_ID: %d\n", paging_message.paging_record[i].UE_ID);
                            printf("    Access Type: %d\n", paging_message.paging_record[i].access_type);

                            if (paging_message.paging_record[i].UE_ID == UE_ID_S)
                            {
                                printf("*******    Paging Record for UE_ID = %d\n", UE_ID_S);
                            }
                        }
                    }
                }
            }
        }
    }
}