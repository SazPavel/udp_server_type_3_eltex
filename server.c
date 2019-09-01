#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#define SIZE 6

pthread_mutex_t ch_ready_lock = PTHREAD_MUTEX_INITIALIZER;
int cycle = 1, sock = -1;
pthread_t tid[SIZE];
pthread_cond_t cond;

void SigintHandler(int sig)
{
    int i;
    cycle = 0;
    pthread_cond_broadcast(&cond);
    for(i = 0; i < SIZE; i++)
    {
        pthread_join(tid[i], NULL);
    }
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&ch_ready_lock);
    printf("Server out\n");
    close(sock);
    exit(0);
}

void *Child_Main(void *ptr)
{
    int *num = (int*)ptr;
    int  fd;
    char buf2[10];
    struct sockaddr_in addr_ready;
    fd = open("./tempfifo", O_RDONLY);
    if(fd < 0)
    {
        perror("openfifoclient");
        exit(-1);
    }
    printf("Thread %d ready\n", *num);
    while(cycle)
    {
        pthread_mutex_lock(&ch_ready_lock);
        pthread_cond_wait(&cond, &ch_ready_lock);
        if(cycle)
        {
            read(fd, &addr_ready, sizeof(addr_ready));
            sprintf(buf2, "%d %s", *num, "say hi\n");
            pthread_mutex_unlock(&ch_ready_lock);
            sendto(sock, buf2, sizeof(buf2), 0, (struct sockaddr *)&addr_ready, sizeof(addr_ready));
            pthread_mutex_lock(&ch_ready_lock);
        }
        pthread_mutex_unlock(&ch_ready_lock);
    }
    printf("Thread %d close\n", *num);
    unlink("./tempfifo");
}

int main()
{
    struct sigaction sigint;
    sigint.sa_handler = SigintHandler;
    sigint.sa_flags = 0;
    sigemptyset(&sigint.sa_mask);
    sigaddset(&sigint.sa_mask, SIGINT);
    sigaction(SIGINT, &sigint, 0);
    int i, fd;
    struct sockaddr_in addr, child;
    socklen_t size = sizeof(child);
    int num[SIZE];
    char buf[16];
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0)
    {
        perror("socket");
        exit(-1);
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(3103);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if(bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        exit(-1);
    }
    listen(sock, SIZE);
    unlink("./tempfifo");
    if(mkfifo("./tempfifo", 0600) < 0)
    {
        perror("mkfifo");
        exit(-1);
    }
    for(i = 0; i < SIZE; i++)
    {
        num[i] = i;
        pthread_create(&tid[i], NULL, Child_Main, &num[i]);
    }
    fd = open("./tempfifo", O_WRONLY);
    if(fd < 0)
    {
        perror("openfifo");
        exit(-1);
    }
    
    while(cycle)
    {
        recvfrom(sock, buf, 16, 0, (struct sockaddr*)&child, &size);
        printf("%s\n", buf);
        write(fd, &child, sizeof(child));
        pthread_cond_signal(&cond);
    }
    exit(0);
}
