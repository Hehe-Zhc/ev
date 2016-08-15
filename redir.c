#include<stdio.h>
#include<unistd.h>
#include<event2/event.h>
#include<sys/socket.h>
#include<sys/types.h>

#define BUFSIZE 100

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
    int errno_save;
    evutil_socket_t listener;

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == -1) return -1;

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(port);

    if (bind(listener, (struct sockaddr *)&sin, sizeof(sin)) < 0 ) {
        return -1;
    }

    listen(listener, listen_num);

    evutil_make_socket_nonblocking(listener);

    return listener;
}

void socket_read_cb(int fd, short event, void *arg)
{
    char msg[BUFSIZE];
    struct event *ev = (struct event *)arg;
    int len = read(fd, msg, sizeof(msg)-1);

    if (len <= 0) {
        printf("read error");
        close(event_get_fd(ev));
        event_free(ev);
        return ;
    }

    msg[len] = 0;
    printf("msg recv: %s\n", msg);
    
    write(fd, msg, len);
}

    

void accept_cb(int fd, short event, void *arg)
{
    evutil_socket_t sockfd;
    
    struct sockaddr_in client;
    socklen_t len = sizeof(client);

    sockfd = accept(fd, (struct sockaddr *)&client, &len);
    evutil_make_socket_nonblocking(sockfd);

    printf("accept a client\n");

    struct event_base *base = (struct event_base*)arg;

    struct event *ev = event_new(NULL, -1, 0, NULL, NULL);
    event_assign(ev, base, sockfd, EV_READ | EV_PERSIST,
        socket_read_cb, (void *)ev);
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
       EV_READ | EV_PERSIST, accept_cb, base);
    event_add(ev_listen, NULL);
    event_base_dispatch(base);

    return 0;
}



