
#include<sys/socket.h>
#include<string.h>
#include<unistd.h>
#include<netinet/in.h>
#include <arpa/inet.h>

#include<event2/event.h>
#include<event2/bufferevent.h>
#include<event2/buffer.h>
#include<event2/util.h>
#include<event2/listener.h>

#include<linux/netfilter_ipv4.h>

#define LISTEN_PORT 9999
#define LISTEN_BAKLOG 1024

/* a very simple TCP proxy server implemented with libevent */
/* client <---> proxy server <----> server */

void connect_to_remote(struct event_base *base, struct sockaddr_in *addr);

struct common_ctx
{
    /* bufferevent of the other side */
    struct bufferevent *other_side;
    /* event base */
    struct event_base *ev_base;
};

/* read callback
   recieve from one side and send to the other side */
void bufev_read_cb(struct bufferevent *bev, void *ctx)
{
    // get the buffer
    struct evbuffer *input = bufferevent_get_input(bev);
    // print the length
    printf("%d bytes received\n", (int)evbuffer_get_length(input));

    // write to the other side
    struct common_ctx *context = (struct common_ctx *)ctx;
    struct bufferevent *bev_other = context->other_side;
    struct evbuffer *output = bufferevent_get_input(bev_other);
    evbuffer_remove_buffer(input, output, evbuffer_get_length(input));
}

/* event callback */
void bufev_event_cb(struct bufferevent *bev, short events, void *ctx)
{
    struct common_ctx *context = (struct common_ctx *)ctx;
    struct bufferevent *bev_other = context->other_side;

    if (events & BEV_EVENT_CONNECTED) {
        printf("connected\n");
    } else if (events & (BEV_EVENT_ERROR | BEV_EVENT_EOF)) {
        // error or EOF, free this bufferevent 
        if (events & BEV_EVENT_EOF) {
            printf("EOF recieved\n");
        } else {
            printf("error\n");
        }
        // this bufferevent
        bufferevent_free(bev);
        // the other side
        bufferevent_flush(bev_other, EV_READ|EV_WRITE, BEV_FINISHED);
        bufferevent_free(bev_other);

    }
}

int get_ori_dest_ip(int fd, struct sockaddr_in *ori_dst_addr)
{
    socklen_t len;
    len = sizeof(struct sockaddr_in);
    getsockopt(fd, SOL_IP, SO_ORIGINAL_DST, ori_dst_addr, &len);

    return 0;
}

void listener_cb(struct evconnlistener *listener, 
                 evutil_socket_t fd,
                 struct sockaddr *sock, 
                 int socklen, 
                 void *ctx)
{
    printf("accept a client %d\n", fd);

    // base from ctx
    struct common_ctx *context = (struct common_ctx *)ctx;
    struct event_base *base = context->ev_base;

    // allocate a bufferevent for this client
    struct bufferevent *bev = 
        bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, bufev_read_cb, NULL, bufev_event_cb, NULL);
    bufferevent_enable(bev, EV_READ | EV_PERSIST);

    // connect to the remote server
    // get original address first
    struct sockaddr_in ori_addr;
    get_ori_dest_ip(fd, &ori_addr);
    connect_to_remote(base, &ori_addr);
}

struct evconnlistener* begin_listening(struct event_base *base)
{
    struct sockaddr_in sin;
    bzero(&sin, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(LISTEN_PORT);

    struct evconnlistener *listener;
    listener = evconnlistener_new_bind(base, listener_cb, base,
        LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE,
        LISTEN_BAKLOG, (struct sockaddr*)&sin, sizeof(struct sockaddr_in));

    return listener;
}

void connect_to_remote(struct event_base *base, struct sockaddr_in *addr)
{
    //allocate a new bufferevent for connection to the remote server
    struct bufferevent *bev;

    // fd is determined after connect(), so provided with -1 here
    bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, bufev_read_cb, NULL, bufev_event_cb, NULL);

    if (bufferevent_socket_connect(bev, (struct sockaddr *)addr, sizeof(*addr) < 0)) {
        fprintf(stderr, "bufferevent_socket_connect error\n");
        bufferevent_free(bev);
    }
}
        
void main_loop()
{
    struct event_base *ev_base;

    ev_base = event_base_new();
    event_base_dispatch(ev_base);
}

int main(void)
{
    main_loop();

    return 0;
}




