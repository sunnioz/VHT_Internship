#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>

#define UE_UDP_PORT 5000
#define AMF_TCP_PORT 6000
#define BUF_SIZE 2048
#define SFN_MAX 1023

#define N 4                 /*So lan gNB gui ban tin RRC_Paging tren 1 chu ki DRX*/
#define DRX_CYCLE 128       /*Chu ki DRX*/
#define PF_OFFSET 0         /*Phan bu PF*/
#define MAXNROFPAGEREC 32   /*So luong ban tin paging record toi da trong 1 ban tin RRC_Paging*/

/*su dung flag_mutex de kiem soat Queue cua Paging Record*/
pthread_mutex_t flag_mutex = PTHREAD_MUTEX_INITIALIZER;

struct UE_message
{
    int     NG_5G_S_TMSI;   /*Dinh danh tam thoi cua UE*/
    char    *payload;       /*Chua noi dung ban tin*/
};

struct NgAP_Paging_message
{
    int     Message_Type;   /*Kieu ban tin*/
    int     NG_5G_S_TMSI;   /*Dinh danh tam thoi cua UE*/
    int     TAI;            /*Vung gNB*/
    int     CN_Domain;      /*Kieu tim goi*/
};

struct Paging_Record
{
    int NG_5G_S_TMSI;       /*Dinh danh tam thoi cua UE*/
    int CN_Domain;          /*Kieu tim goi*/
};

struct RRC_Paging_message
{
    int Message_Type;       /*Kieu ban tin*/
    int num_Paging_record;  /*So luong ban tin paging record*/
    struct Paging_Record paging_record[MAXNROFPAGEREC];
};

struct MIB
{
    char message_id;        /*Dinh danh cua ban tin*/
    short sfn_value;        /*Gia tri SFN*/
};

struct SIB1
{
    short s_PF_OFFSET;      /*Phan bu PF*/
    short s_DRX_CYCLE;      /*Chu ki DRX*/
    short s_N;              /*So lan gNB gui ban tin RRC_Paging tren 1 chu ki DRX*/
};

/*Anh xa Paging_record toi group tuong ung*/
struct mapping_paging_record_to_group 
{
    struct Paging_Record paging_record;
    int group;
};


/*Khoi tao Queue cap phat dong luu ban tin Paging_Record*/
typedef struct node {
    struct mapping_paging_record_to_group data;
    struct node *next;
} node;

/*
 *  *make_node(struct mapping_paging_record_to_group x)
 *   return ve mot node moi duoc tao ra
 */
node 
*make_node(struct mapping_paging_record_to_group x)
{
    node *new_node = (node *)malloc(sizeof(node));
    new_node->data = x;
    new_node->next = NULL;
    return new_node;
}
/*
 *  *push(node **queue, struct mapping_paging_record_to_group x)
 *   them mot node vao cuoi queue
 */
void
push(node **queue, struct mapping_paging_record_to_group x)
{
    node *new_node = make_node(x);
    if(*queue == NULL){
        *queue = new_node;
    }
    else{
        node *temp = *queue;
        while(temp->next != NULL){
            temp = temp->next;
        }
        temp->next = new_node;
    }
}

/*
 *  *pop(node **queue)
 *   xoa node dau tien cua queue
 */
void 
pop(node **queue)
{
    if(*queue == NULL){
        return;
    }
    node *temp = *queue;
    *queue = (*queue)->next;
    free(temp);
}

/*
 *  *size(node *queue)
 *   tra ve so luong node trong queue
 */
int 
size(node *queue)
{
    int count = 0;
    node *temp = queue;
    while(temp != NULL){
        count++;
        temp = temp->next;
    }
    return count;
}

/*
 *  *empty(node *queue)
 *   kiem tra queue co rong hay khong
 */
bool 
empty(node *queue)
{
    return queue == NULL;
}

/*
 *  *duyet(node *queue)
 *   in ra cac node trong queue
 */
void 
duyet(node *queue)
{
    node *temp = queue;
    while(temp != NULL){
        printf("Group: %d\n", temp->data.group);
        printf("TMSI: %d,CN_DOMAIN %d\n", temp->data.paging_record.NG_5G_S_TMSI, temp->data.paging_record.CN_Domain);
        temp = temp->next;
    }
}

/*
 *  *filter_rrc_paging_messages(node **queue, int group)
 *   loc cac ban tin paging record cua group tuong ung co trong queue
 */
struct RRC_Paging_message
filter_rrc_paging_messages(node **queue, int group)
{
    struct RRC_Paging_message rrc_paging_message;
    rrc_paging_message.Message_Type = 101;
    rrc_paging_message.num_Paging_record = 0;

    node *temp = *queue;
    node *prev = NULL;

    while (temp != NULL) {
        if (temp->data.group == group) {
            rrc_paging_message.paging_record[rrc_paging_message.num_Paging_record] = temp->data.paging_record;
            rrc_paging_message.num_Paging_record++;
            if(rrc_paging_message.num_Paging_record == MAXNROFPAGEREC) break;       /*chi lay toi da MAXNROFPAGEREC ban tin co trong Queue*/
            if (prev == NULL) {
                *queue = temp->next;
            } else {
                prev->next = temp->next;
            }
            node *to_delete = temp;
            temp = temp->next;
            free(to_delete);
        } else {
            prev = temp;
            temp = temp->next;
        }
    }
    return rrc_paging_message;
}


node *queue_of_Paging_record = NULL;            /*Queue luu tru cac ban tin Paging Record*/
int PF_first_s[N];                              /*Mang luu tru cac gia tri PF dau tien cua moi chu ky DRX tuong ung voi nhom UE_ID*/
struct sockaddr_in group_cliaddr[N] = {0};      /*Mang luu tru dia chi cua cac nhom UE_ID*/
volatile short gNodeB_sfn = 0;                  /*Bien luu tru gia tri SFN*/
struct SIB1 sib1 = {PF_OFFSET, DRX_CYCLE, N};   /*Bien luu tru thong tin SIB1*/
struct NgAP_Paging_message NgAP_paging_message; /*Bien luu tru thong tin ban tin NgAP_Paging_message*/
struct RRC_Paging_message rrc_paging_message;   /*Bien luu tru thong tin ban tin RRC_Paging_message*/


/*
 *  *init_rrc_paging_message()
 *   khoi tao ban tin RRC_Paging_message
 */
void 
init_rrc_paging_message()
{
    rrc_paging_message.Message_Type = 101;
    rrc_paging_message.num_Paging_record = 0;
}

/*
 *  *reset_rrc_paging_message()
 *   reset ban tin RRC_Paging_message
 */
void 
reset_rrc_paging_message()
{
    memset(&rrc_paging_message, 0, sizeof(rrc_paging_message));
}

/*
 *  *print_client_info(struct sockaddr_in *cliaddr)
 *   in ra dia chi IP va port cua client
 */
void 
print_client_info(struct sockaddr_in *cliaddr)
{

    char *client_ip = inet_ntoa(cliaddr->sin_addr);
    int client_port = ntohs(cliaddr->sin_port);
    printf("Client IP: %s\n", client_ip);
    printf("Client Port: %d\n", client_port);
}

/*
 *  *add_paging_record_to_queue(struct NgAP_Paging_message paging_message)
 *   chuyen cac ban tin NgAP_Paging_message thanh cac ban tin Paging Record tuong ung
 *   them ban tin Paging Record vao queue
 */
void 
add_paging_record_to_queue(struct NgAP_Paging_message paging_message)
{
    struct Paging_Record paging_record;
    paging_record.NG_5G_S_TMSI = paging_message.NG_5G_S_TMSI;
    paging_record.CN_Domain = paging_message.CN_Domain;
    int UE_ID = paging_message.NG_5G_S_TMSI%1024;
    int group = UE_ID % N;
    if(group_cliaddr[group].sin_family == 0) return;
    struct mapping_paging_record_to_group x;
    x.paging_record = paging_record;
    x.group = group;
    push(&queue_of_Paging_record, x);
}

// signal handler function
void 
increment_sfn(int signum)
{
    gNodeB_sfn = (gNodeB_sfn + 1) % 1024;
}
/*
 *  *setup_timer()
 *   cai dat timer de tang gia tri SFN
 */
void 
setup_timer()
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

/*
 *  *cal_PF_first_s()
 *  tinh toan gia tri PF dau tien cua moi chu ky DRX tuong ung voi nhom UE_ID
 *  (SFN + PF_OFFSET)%T = (T / N) * (UE_ID % N);
 */

void 
cal_PF_first_s()
{
    for (int i = 0; i < N; i++)
    {
        PF_first_s[i] = (DRX_CYCLE / N) * i;
    }
}

/*
 *  *udp_server(void *arg)
 *   tao socket udp de ket noi voi UE
 *   nhan ban tin UE_ID_S tu UE
 *   gui ban tin SIB1 den UE
 *   gui ban tin RRC_Paging den UE
 */
void 
*udp_server(void *arg)
{
    int udp_sockfd;
    struct sockaddr_in servaddr, cliaddr;
    char buffer[BUF_SIZE];
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
        int n = recvfrom(udp_sockfd, (char *)buffer, BUF_SIZE, MSG_DONTWAIT, (struct sockaddr *)&cliaddr, &len);

        /*
         *  nhan ban tin UE_ID_S tu UE
         *  luu tru dia chi IP va port cua UE vao cac nhom tuong ung
         *  gui ban tin SIB1 den UE
         */
        if (n > 0)
        {
            struct UE_message ue_message;
            memcpy(&ue_message, buffer, sizeof(ue_message));
            printf("UE_ID_S received from UE: %d\n", ue_message.NG_5G_S_TMSI);
            int UE_ID = ue_message.NG_5G_S_TMSI % 1024;
            group_cliaddr[UE_ID % N] = cliaddr;
            print_client_info(&group_cliaddr[UE_ID % N]);
            memcpy(buffer, &sib1, sizeof(sib1));
            sendto(udp_sockfd, buffer, sizeof(sib1), MSG_CONFIRM, (const struct sockaddr *)&cliaddr, len);
            printf("SIB1 message sent to UE\n");
        }

        /*
         * vao cac thoi diem PF tien hanh gui cac ban tin tuong ung      
         */
        pthread_mutex_lock(&flag_mutex);
        for(int i = 0; i < N;i++)
        {
            if(group_cliaddr[i].sin_family == 0) continue;
            for(int j = PF_first_s[i]; j <= SFN_MAX; j += DRX_CYCLE)
            {
                if(gNodeB_sfn == j)
                {
                    rrc_paging_message = filter_rrc_paging_messages(&queue_of_Paging_record, i);
                    if(rrc_paging_message.num_Paging_record == 0 ) continue;
                    sendto(udp_sockfd, (char *)&rrc_paging_message, sizeof(rrc_paging_message), MSG_CONFIRM, (const struct sockaddr *)&group_cliaddr[i], len);
                    printf("gNodeB_sfn: %d RRC Paging message sent to UE: %d\n",gNodeB_sfn, i);
                    reset_rrc_paging_message();
                }
            }
        }
        pthread_mutex_unlock(&flag_mutex);

        /*
         * Tien hanh gui ban tin MIB nham muc dung dong bo SFN
         */
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
            for(int i = 0; i < N;i++)
            {
                if(group_cliaddr[i].sin_family == 0) continue;
                sendto(udp_sockfd, buffer, sizeof(mib), MSG_CONFIRM, (const struct sockaddr *)&group_cliaddr[i], len);
            }
        }
    }
    return NULL;
}

/*
 *  *tcp_server(void *arg)
 *   tao socket tcp de ket noi voi AMF
 *   nhan ban tin NgAP_Paging_message tu AMF
 */
void 
*tcp_server(void *arg)
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
            if(buffer[0] == 100)                                    /*nhan ban tin NgAP_Paging_message tu AMF*/
            {
                memcpy(&NgAP_paging_message, buffer, sizeof(NgAP_paging_message));
                printf("gNodeB:%d,Received paging message from AMF: UE_ID = %d, TAC = %d, CN_Domain = %d\n", gNodeB_sfn, NgAP_paging_message.NG_5G_S_TMSI, NgAP_paging_message.TAI, NgAP_paging_message.CN_Domain);
                pthread_mutex_lock(&flag_mutex);
                add_paging_record_to_queue(NgAP_paging_message);    /*them ban tin Paging Record vao Queue*/
                duyet(queue_of_Paging_record);                      /*in ra cac ban tin Paging Record trong Queue*/
                pthread_mutex_unlock(&flag_mutex);
            }
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
