#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <WS2tcpip.h>
//#pragma comment(lib, "ws2_32.lib")        //codeblock 사용 시 필요

typedef struct ListNode
{
    const char* data;    //client에선 client 정보, cache에선 홈페이지
    struct ListNode* link;   //client와 cache에서 단순 연결리스트
    FILE* html_fd;   //cache에서 사용
    struct ListNode* p;  //client에서 사용 (사용하지 않을 수도 있기 때문에 일단 보류)
    int count;   //cache에서 빈도수, client 캐시의 갯수
}ListNode;

void error(char* message);
char* sendRequest(char inputValueBuffer[10000], int* valueIsUpdated);
ListNode* search(ListNode* head, char* x);  //x값을 data로 갖는 포인터를 찾는 함수
ListNode* BeforeSearch(ListNode* head, char* x);    //x값을 data로 갖는 포인터의 선행 포인터를 찾는 함수
void remove_node(ListNode** phead, ListNode* p, ListNode* removed);
void insert(ListNode** phead, ListNode* p, ListNode* new_node);
ListNode* MinCount(ListNode* head);    //리스트의 data 값 중에서 가장 작은 값을 가진 포인터를 찾는 함수

int main(int argc, char* argv[])
{
    WSADATA wsaData;
    SOCKET sockfd, newsockfd;
    SOCKADDR_IN serv_addr, cli_addr;
    char message[256];

    ListNode* client_list, * cache_list, * before_p;
    int portno, clilen, yn;
    char buffer[256], in_num[256], * get_request = "GET / HTTP/1.1\r\nHost:", * carriage_returns = "\r\n\r\n";    //strings for creating user get requests
    char requ_buffer[10000];
    char full_request[300];    //300임의로 설정
    char exitCompare[256];
    int responseValue;    //used to get 200 OK or other responses
    char* cacheContent;
    int cli_n = 0;

    if (argc != 2)
    {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        error("WSAStartup() error");

    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd == INVALID_SOCKET)
        error("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(sockfd, (SOCKADDR*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR)
        error("bind() error");

    if (listen(sockfd, 5) == SOCKET_ERROR)
        error("listen() error");

    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (SOCKADDR*)&cli_addr, &clilen);

    if (newsockfd == INVALID_SOCKET)
        error("accept() error");

    //소켓을 설정을 해준다.
    ListNode* client_node = (ListNode*)malloc(sizeof(ListNode));
    client_node->data = cli_n;
    insert(&client_list, NULL, client_node);

    //클라이언트가 서버와 성공적으로 연결이 되었기 때문에 data에 클라이언트 번호를 가지고 있는 클라이언트 노드를 생성한다.
    yn = send(newsockfd, cli_n, strlen(cli_n), 0);    //클라이언트를 구분하기 위해 서버 측에서 클라이언트에게 클라이언트 고유 번호를 전해준다.
    cli_n++;    //클라이언트에게 번호를 주었기 때문에 서버는 번호를 증가시켜 다음 클라이언트에게 줄 번호를 준비한다.

    while (1)    //한 서버에 다중 클라이언트가 접속이 가능하기 때문에 서버는 항상 열어둔다.
    {
        memset(&buffer, 0, 256);
        yn = recv(newsockfd, in_num, 255, 0);    //서버가 클라이언트에게 준 클라이언트 고유 번호를 받는다.
        if (yn < 0)
            error(EXIT_FAILURE, 0, "ERROR reading from socket");

        //클라이언트로부터 접속한 홈페이지에 대한 메세지를 받는다.
        memset(&buffer, 0, 256);
        yn = recv(newsockfd, buffer, 255, 0);
        if (yn < 0)
            error(EXIT_FAILURE, 0, "ERROR reading from socket");

        //클라이언트가 서버와의 접속을 종료하기를 원하는지 확인
        memset(&exitCompare, 0, 256);
        strncpy(exitCompare, buffer, strlen(buffer));
        if (strcmp(exitCompare, "quit") == 0)    //클라이언트가 보낸 메세지에 종료의 의미가 포함되면
        {
            before_p = BeforeSearch(client_list, in_num);    //삭제를 위해 클라이언트 번호를 가지고 있는 포인터의 선행 포인터를 찾는다.
            remove_node(&client_list, before_p, client_node);    //종료를 원하는 클라이언트의 번호를 가지고 있는 노드를 삭제한다.
            printf("\nThe client has disconnected. Waiting for a new client...\n");
            closesocket(newsockfd);

            //새로운 클라이언트와 접속
            listen(sockfd, 5);
            clilen = sizeof(cli_addr);
            newsockfd = accept(sockfd, (struct sockaddr*) & cli_addr, &clilen);
            if (newsockfd < 0)
            {
                error(EXIT_FAILURE, 0, "ERROR on accept");
            }

            ListNode* client_node = (ListNode*)malloc(sizeof(ListNode));
            client_node->data = cli_n;
            insert(&client_list, NULL, client_node);
            yn = send(newsockfd, cli_n, strlen(cli_n), 0);
            cli_n++;
            printf("Client has connected...\n");
            memset(&in_num, 0, 255);
            memset(&buffer, 0, 255);
            memset(&exitCompare, 0, 255);
        }
        else    //클라이언트가 보낸 메세지에 종료의 의미가 없으면
        {
            printf("\nClient has requested for the webpage: %s\n", buffer);

            char* returnValue = sendRequest(buffer, &responseValue);

            sprintf(requ_buffer, "%s", returnValue);

            if (strstr(requ_buffer, "200 OK") != NULL)
            {
                char ch;
                long lSize;
                char cacheListBuffer[256];
                char copyString[256];

                //해당 클라이언트 번호를 가진 클라이언트 노드를 캐시 리스트로 정의한다.
                cache_list = search(client_list, in_num);

                //buffer 값의 홈페이지를 가진 캐시를 찾아서 정의한다.
                ListNode* cache = search(cache_list, buffer);
                strcpy(copyString, buffer);
                sprintf(copyString + strlen(copyString), ".html");

                if (cache != NULL)
                {
                    printf("\nThe web page is already stored in the cache.\n");
                    cache->count++;
                    cache->html_fd = fopen(copyString, "r+");

                    fprintf(cache->html_fd, "%s", requ_buffer);
                    rewind(cache->html_fd);

                    fseek(cache->html_fd, 0L, SEEK_END);
                    lSize = ftell(cache->html_fd);
                    rewind(cache->html_fd);

                    //allocate memory for entire content
                    cacheContent = calloc(1, lSize + 1);

                    //copy the file into the buffer
                    if (1 != fread(cacheContent, lSize, 1, cache->html_fd))
                        fclose(cache->html_fd), free(cacheContent), fputs("entire read fails", stderr), exit(1);

                    fclose(cache->html_fd);

                    //Send the contents of the file, now in a buffer, to the client
                    yn = send(newsockfd, cacheContent, strlen(cacheContent), 0);
                    printf("\nThe web page is successfully sent to the client.\n");

                    memset(&copyString, 0, 256);
                    free(cacheContent);
                }
                else    //If cache is NULL (buffer 홈페이지를 처음 접속했다는 의미)
                {
                    if (cache_list->count > 4)
                    {
                        //최저빈도수 찾아 삭제
                        ListNode* LFU_cache = MinCount(cache_list);
                        before_p = BeforeSearch(cache_list, LFU_cache->count);
                        remove_node(&cache_list, before_p, LFU_cache);
                    }

                    //cache_list->count는 4보다 무조건 작고 cache의 data는 탐색되지 않음
                    //cache_node 생성 삽입
                    ListNode* cache_node = (ListNode*)malloc(sizeof(ListNode));
                    cache_node->data = buffer;
                    cache_node->count = 1;
                    insert(&cache_list, NULL, cache_node);

                    //Sends the contents of the .html cache file to the client
                    cache->html_fd = fopen(copyString, "w+");
                    printf("\nThe web page is successfully caches.\n");

                    fprintf(cache->html_fd, "%s", requ_buffer);
                    rewind(cache->html_fd);

                    fseek(cache->html_fd, 0L, SEEK_END);
                    lSize = ftell(cache->html_fd);
                    rewind(cache->html_fd);

                    //allocate memory for entire content
                    cacheContent = calloc(1, lSize + 1);

                    //copy the file into the buffer
                    if (1 != fread(cacheContent, lSize, 1, cache->html_fd))
                        fclose(cache->html_fd), free(cacheContent), fputs("entire read fails", stderr), exit(1);

                    fclose(cache->html_fd);

                    //Send the contents of the file, now in a buffer, to the client
                    yn = send(newsockfd, cacheContent, strlen(cacheContent), 0);
                    printf("\nThe web page is successfully sent to the client.\n");

                    memset(&copyString, 0, 256);
                    free(cacheContent);
                }
            }
            else    //If the status code is not "200 OK", send the webpage directly to the client
            {
                yn = recv(newsockfd, requ_buffer, strlen(requ_buffer), 0);    //buffer1의 내용을 newsockfd에 전송(쓰기)
                printf("\nHTTP status code is not 200. Page is not cached. Information is directly sent tot he client.\n");

                memset(requ_buffer, 0, 10000);
                memset(buffer, 0, 256);
            }
        }    //End of if statement for if the status code is "200 OK"
    }    //End inner while loop

    return 0;
}

void error(char* message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

char* sendRequest(char inputValueBuffer[], int* valueIsUpdated)
{
    struct addrinfo hints, * results;
    int checkValue;
    int sockfd;
    char* user_input;
    static char buffer[10000];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    memset(buffer, 0, 10000);

    if ((checkValue = getaddrinfo(inputValueBuffer, "80", &hints, &results)) != 0)    //http 네트워크 주소 정보를 가져오는 함수
    {
        return inputValueBuffer;    //네트워크 주소 정보를 가져오지 못한 경우
    }

    sockfd = socket(results->ai_family, results->ai_socktype, results->ai_protocol);

    connect(sockfd, results->ai_addr, results->ai_addrlen);
    send(sockfd, "GET / HTTP/1.1\r\n\r\n", 23, 0);    //http 헤더 형식 필수요소 => 요청 메서드, 요청 URI(자원위치), 프로토콜 버전
    //https://www.joinc.co.kr/w/Site/Network_Programing/AdvancedComm/HTTP

    int recv_length = 1;
    recv_length = recv(sockfd, buffer, 10000, 0);  

    return buffer;
}

//탐색
ListNode* search(ListNode* head, char* x)
{
    ListNode* p;
    p = head;
    while (p != NULL)
    {
        if (strcmp(p->data, x) == 0)
            return p;
        p = p->link;
    }
    return p;
}

//이전 탐색
ListNode* BeforeSearch(ListNode* head, char* x)
{
    ListNode* p;
    p = head;
    if (p == NULL)
        return p;
    while (p->link != NULL)
    {
        if (strcmp(p->link->data, x) == 0)
            return p;
        p = p->link;
    }
    return p;
}

//삭제
void remove_node(ListNode** phead, ListNode* p, ListNode* removed)
{
    if (p == NULL)
        *phead = (*phead)->link;
    else
        p->link = removed->link;
    free(removed);
    (*phead)->count--;
}

//삽입
void insert(ListNode** phead, ListNode* p, ListNode* new_node)
{
    if (*phead == NULL)
    {
        new_node->link = NULL;
        *phead = new_node;
    }
    else if (p == NULL)
    {
        new_node->link = *phead;
        *phead = new_node;
    }
    else
    {
        new_node->link = p->link;
        p->link = new_node;
    }
    (*phead)->count++;
}

//최소 빈도수 포인터 탐색
ListNode* MinCount(ListNode* head)
{
    ListNode* p, * q;
    p = head->link;
    q = p->link;
    ListNode* LFU = p;
    while (q != NULL)
    {
        if (p->count > q->count)
            LFU = q;
        p = p->link;
        q = q->link;
    }
    return LFU;
}
