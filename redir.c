#include<stdio.h>
#include<unistd.h>
#include<event2/event.h>
#include<sys/socket.h>
#include<sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include<linux/netfilter_ipv4.h>
#include<string.h>
#include<errno.h>
#include<stdlib.h>

#define BUFSIZE 102400

struct arg_info {
    struct  event *ev;
    int fd;
};

int tcp_connect_server(const char *server_ip, int port);

int tcp_connect_server(const char *server_ip, int port)
{
    int sockfd, status;
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    status = inet_aton(server_ip, &server_addr.sin_addr);

    if (status == 0) {
        errno = EINVAL;
        return 1;
    }

    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        return sockfd;

    status = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (status == -1) {
        printf("connect error");
        return -1;
    } else {
        //printf("connect to %s success\n", inet_ntoa(server_addr));
        ;
    }

    evutil_make_socket_nonblocking(sockfd);
    return sockfd;
}

void cmd_msg_cb(int fd, short event, void *args)
{
    char buf[BUFSIZE];
    int ret;
    ret = read(fd, buf, BUFSIZE);
    if (ret < 0) {
        printf("read error\n");
    }
    buf[ret] = '\0';
    printf("%s\n", buf);

}

int tcp_server_init(int port, int listen_num)
{
    evutil_socket_t listener;

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == -1) return -1;

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    inet_aton("0.0.0.0", &sin.sin_addr);
    sin.sin_port = htons(port);

    if (bind(listener, (struct sockaddr *)&sin, sizeof(sin)) < 0 ) {
        return -1;
    } else {
        ;
        //printf("listening on %s ...\n", inet_ntoa(sin.sin_addr));
    }

    listen(listener, listen_num);

    evutil_make_socket_nonblocking(listener);

    return listener;
}

void client_read_cb(int fd, short event, void *args)
{
    char buf[BUFSIZE];
    struct arg_info *arginfo = (struct arg_info *)args;
    
    int len;
    len = read(fd, buf, BUFSIZE);
    if (len == 0) {
        printf("client EOF read, close the connection\n");
        close(event_get_fd(arginfo->ev));
        event_free(arginfo->ev);
        close(arginfo->fd);
        return ;
    } else if (len < 0) {
        printf("read error, close the connection");
        close(event_get_fd(arginfo->ev));
        event_free(arginfo->ev);
        return ;
    }

    // write to the server's socket
    printf("client recieve msg len: %d\n", len);
    write(arginfo->fd, buf, len);
}



void server_socket_read_cb(int fd, short event, void *arg)
{
    char msg[BUFSIZE];
    struct arg_info *ainfo = (struct arg_info *)arg;
    int len;

    len = read(fd, msg, BUFSIZE);
    struct event *ev = ainfo->ev;
    if (len == 0) {
        printf("server EOF read, close the connection\n");
        close(event_get_fd(ev));
        event_free(ev);
        return ;
    } else if (len < 0) {
        printf("read error, close the connection");
        close(event_get_fd(ev));
        event_free(ev);
        return ;
    }

    msg[len] = 0;
    printf("server recieve msg len: %d\n", len);
    
    write(ainfo->fd, msg, len);
}

int get_ori_dest_ip(int fd, struct sockaddr_in *ori_dst_addr)
{
    int len;
    len = sizeof(struct sockaddr_in);
    getsockopt(fd, SOL_IP, SO_ORIGINAL_DST, ori_dst_addr, &len);

    return 0;
}

void server_accept_cb(int fd, short event, void *arg)
{
    evutil_socket_t sockfd;
    
    struct sockaddr_in client;
    struct sockaddr_in ori_addr;
    socklen_t len = sizeof(client);

    sockfd = accept(fd, (struct sockaddr *)&client, &len);
    evutil_make_socket_nonblocking(sockfd);
    get_ori_dest_ip(sockfd, &ori_addr);
    printf("accept a client\n");
    printf("client addr: %s\n", inet_ntoa(client.sin_addr));
    printf("client port: %d\n", ntohs(client.sin_port));
    printf("ori dst addr: %s\n", inet_ntoa(ori_addr.sin_addr));
    printf("ori dst port: %d\n", ntohs(ori_addr.sin_port));

    // if redir, connect to the ori_addr
    int cli_sockfd;
    struct arg_info *args1;
    args1 = malloc(sizeof(struct arg_info));
    if (strncmp("127", inet_ntoa(ori_addr.sin_addr), 3) != 0) {
        printf("redirect to original address\n");
        //cli_sockfd = tcp_connect_server(inet_ntoa(ori_addr.sin_addr), ntohs(ori_addr.sin_port));
        cli_sockfd = tcp_connect_server("192.168.8.1", ntohs(ori_addr.sin_port));
        struct event_base *evbase = (struct event_base*)arg;
        struct event *cli_ev = event_new(NULL, -1, 0, NULL, NULL);
        args1->ev = cli_ev;
        args1->fd = sockfd;
        event_assign(cli_ev, evbase, cli_sockfd, EV_READ | EV_PERSIST,
            client_read_cb, (void *)args1);
        event_add(cli_ev, NULL);


    }


    struct event_base *base = (struct event_base*)arg;
    struct event *ev = event_new(NULL, -1, 0, NULL, NULL);
    struct arg_info *args;
    args = malloc(sizeof(struct arg_info));
    args->ev = ev;
    args->fd = cli_sockfd;
    event_assign(ev, base, sockfd, EV_READ | EV_PERSIST,
        server_socket_read_cb, (void *)args);
    event_add(ev, NULL);
}


int main2()
{
    struct event_base *ev_base;
    struct event *ev;
    int fd;

    fd = STDIN_FILENO;

    ev_base = event_base_new();
    ev = event_new(ev_base, fd, EV_READ | EV_PERSIST, 
      cmd_msg_cb, NULL);
    event_add(ev, NULL);

    event_base_dispatch(ev_base);
    return 0;
}

int main()
{
    int listener = tcp_server_init(9999, 10);
    if (listener == -1) 
    {
        printf("listener error\n");
        return -1;
    }
    struct event_base *base = event_base_new();
    struct event *ev_listen = event_new(base, listener,
       EV_READ | EV_PERSIST, server_accept_cb, base);
    event_add(ev_listen, NULL);
    event_base_dispatch(base);

    return 0;
}



