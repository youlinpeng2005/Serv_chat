#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <sys/stat.h>
#include <sys/types.h>

#define SERV_PORT 5888
#define MAX_CLIENT 1024
#define MAX_FRIEND 99
#define MAX_NAME 9

typedef enum
{
    STATE_WAIT_LOGIN,
    STATE_WAIT_PASSWORD,
    STATE_ONLINE,
    STATE_OFFLINE
} ClientState;

typedef struct Client
{
    char username[64];
    char password[64];
    ClientState state;
    int friendNUM;
    struct Client* friends[MAX_FRIEND];
    struct bufferevent* bev;
} Client;


Client* Database[MAX_CLIENT];
int data_flog = 0;
//char Tips[] = "Enter ![friend_name] to chat \"!add\" to add friends, \"!del\" to delete friends, and \"!list\" to view the friend list and friend status\n";

int find_user_index(const char* name)
{
    for (int i = 0; i < data_flog; i++) {
        if (strcmp(Database[i]->username, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

int matchUsername(Client* client)
{
    printf("username: %s\n", client->username);
    for (int index = 0; index < data_flog; index++)
    {
        if (strcmp(client->username, Database[index]->username) == 0)
        {
            return index;
        }
    }
    return -1;
}

void turnUserStateInServ(int index, ClientState state)
{
    Database[index]->state = state;
    printf("The user: %s has changed the network status: %d\n", Database[index]->username, Database[index]->state);
}

int Check_if__have_friend(Client* client,int user_index, int user_friend_index)
{
    for (int i = 0; i < Database[user_index]->friendNUM; i++)
    {
        if (strcmp(Database[user_index]->friends[i]->username,Database[user_friend_index]->username) == 0)
        {
            for (int j = 0; j < Database[user_friend_index]->friendNUM; j++)
            {
                if (strcmp(Database[user_friend_index]->friends[j]->username,Database[user_index]->username) == 0)
                {
                    return 1;
                }
                else
                {
                    return 0;
                }
            }
        }
    }
    return 0;
}

char* Check_friend_status(int index)
{
    char* status = NULL;
    if (Database[index]->state == 2)
    {
        status = " status: online\n";
    }
    else if (Database[index]->state == 3)
    {
        status = " status: offline\n";
    }
    else
        status = " status: unknown\n";
    return status;
}

void Add_friend(Client* client,const char* friendName, int index)
{
    int i = find_user_index(friendName);
    if (i == -1)
    {
        char* error_tips = "The user could not be found. Please confirm whether the friend's username is correct.\n";
        bufferevent_write(client->bev, error_tips, strlen(error_tips));
    }
    else if (i == index)
    {
        char* error_tips = "Please do not add yourself as a friend!!!\n";
        bufferevent_write(client->bev, error_tips, strlen(error_tips));
    }
    else
    {
        if (Check_if__have_friend(client, index, i) == 0)
        {
            Database[index]->friends[Database[index]->friendNUM] = Database[i];
            Database[index]->friendNUM++;
            printf("Client: %s add a friend who named %s\n", Database[index]->username, Database[i]->username);
            char* tip1 = "Add friend successfully: ";
            char* tip2 = "If you want to start the chat service, your friends need to add you before you can start.\n";
            char* friend_name = Database[i]->username;
            char result[256];
            snprintf(result, sizeof(result), "%s%s\n", tip1, friend_name);
            bufferevent_write(client->bev, result, strlen(result));
            bufferevent_write(client->bev, tip2, strlen(tip2));

            if (Database[i]->bev != NULL)
            {
                char* notice_prefix = "The user: ";
                char* notice_suffix = " has added you to their friends list\n";
                snprintf(result, sizeof(result), "%s%s%s", notice_prefix, Database[index]->username, notice_suffix);
                bufferevent_write(Database[i]->bev, result, strlen(result));
                bufferevent_write(Database[i]->bev, tip2, strlen(tip2));
            }
            else
            {
                printf("Error: The friend %s is not online or has no active connection.\n", Database[i]->username);
            }
        }
        else
        {
            char* tip1 = "You already have this friend, please do not add the friend again\n";
            bufferevent_write(client->bev, tip1, strlen(tip1));
        }
        
    }
}

void Del_friend(Client* client, const char* friendName, int index)
{
    int i = find_user_index(friendName);
    if (i == -1)
    {
        char* error_tips = "The user could not be found. Please confirm whether the friend's username is correct.\n";
        bufferevent_write(client->bev, error_tips, strlen(error_tips));
    }
    else
    {
        int found = 0;
        for (int j = 0; j < Database[index]->friendNUM; ++j)
        {
            if (Database[index]->friends[j] == Database[i])
            {
                for (int k = j; k < Database[index]->friendNUM - 1; ++k)
                {
                    Database[index]->friends[k] = Database[index]->friends[k + 1];
                }
                Database[index]->friendNUM--;
                found = 1;
                break;
            }
        }

        if (found)
        {
            printf("Client: %s deleted a friend who named %s\n", Database[index]->username, Database[i]->username);
            char* tips = "Delete friend successfully: ";
            char* friend_name = Database[i]->username;
            char result[256];
            snprintf(result, sizeof(result), "%s%s\n", tips, friend_name);
            bufferevent_write(client->bev, result, strlen(result));
        }
        else
        {
            char* not_found = "The user is not in your friend list.\n";
            bufferevent_write(client->bev, not_found, strlen(not_found));
        }
    }
}

int delA(Client* client) //注销账号
{

    for (int i = 0; i < data_flog; ++i) 
    {
        if (strcmp(Database[i]->username, client->username) == 0) 
        {
            //bufferevent_write(client->bev, "Your account has been successfully deleted.\n", 45);
            //bufferevent_write(client->bev, "Please press Enter to continue\n", 33);
            free(Database[i]);
            for (int j = i; j < data_flog - 1; ++j) 
            {
                Database[j] = Database[j + 1];
            }
            data_flog--;
            return 1;
        }
    }
    return 0;
}

void Help_documentation(Client* client)
{
    const char* help_documentation[] = 
    {
        "!add friend name like !add pyl\n",
        "!del friend name like !del pyl\n",
        "!list can view friends list and status\n",
        "!friendName can chat with your friend like !pyl hello PYL_CHAT\n",
        "!delA can cancel your account\n"
    };

    for (int i = 0; i < 5; i++)
    {
        bufferevent_write(client->bev, help_documentation[i], strlen(help_documentation[i]));
    }
}


int sendMessage(Client* client, char* buf,int index)
{
    printf("Client:%s want to send message\n", client->username);
    char friend_name[MAX_NAME] = { 0 };
    int i = 1, j = 0;
    while (buf[i] != ' ' && buf[i] != '\0' && buf[i] != '\n' && j < MAX_NAME - 1)
    {
        friend_name[j++] = buf[i++];
    }
    friend_name[j] = '\0';
    if (buf[i] != ' ') 
    {
        char* msg = "Error: No message content found after friend name\n";
        bufferevent_write(client->bev, msg, strlen(msg));
        return 0;
    }
    i++;
    char message[1024] = { 0 };
    strncpy(message, &buf[i], sizeof(message) - 1);
    printf("friend: %s message: %s\n", friend_name,message);
    int friend_index = find_user_index(friend_name);
    if (Check_if__have_friend(client,index, friend_index))
    {
        if (Database[friend_index]->state == 2)
        {
            char messageBuffer[BUFSIZ];
            snprintf(messageBuffer, sizeof(messageBuffer), "%s: %s\n", client->username, message);
            bufferevent_write(Database[friend_index]->bev, messageBuffer, strlen(messageBuffer)); 
            //bufferevent_write(Database[friend_index]->bev, message, strlen(message));
            return 1;
        }
        else if (Database[friend_index]->state == 3)
        {
            char* errtips = "The friend is not online at the moment, message sending failed\n";
            bufferevent_write(client->bev, errtips, strlen(errtips));
            return 0;
        }
    }
    else
    {
        char* errtips = "You don't have the other person's friends yet\n";
        bufferevent_write(client->bev, errtips, strlen(errtips));
        return 0;
    }
    return 0;
}

int judgmentCommand(char* buf,Client* client)
{
    char* command = NULL;
    command = (char*)malloc(sizeof(char) * 6);

    for (int i = 0; i < 5; i++)
    {
        command[i] = buf[i];
    }
    command[5] = '\0';
    if (strcmp(command, "!add ") == 0) //添加好友
    {
        printf("Client:%s want to add friend\n", client->username);
        int index = matchUsername(client);
        char friend_name[MAX_NAME] = { 0 };
        int j = 0;
        for (int i = 5; buf[i] != '\0' && buf[i] != '\n' && j < MAX_NAME - 1; i++, j++) 
        {
            friend_name[j] = buf[i];
        }
        friend_name[j] = '\0';
        printf("Friend name: %s\n", friend_name);

        Add_friend(client,friend_name, index);
        free(command);
        return 1;
    }

    if (strcmp(command, "!del ") == 0) //删除好友
    {
        printf("Client:%s want to del friend\n", client->username);
        int index = matchUsername(client);
        char friend_name[MAX_NAME] = { 0 };
        int j = 0;
        for (int i = 5; buf[i] != '\0' && buf[i] != '\n' && j < MAX_NAME - 1; i++, j++)
        {
            friend_name[j] = buf[i];
        }

        friend_name[j] = '\0';
        printf("Friend name: %s\n", friend_name);
        Del_friend(client, friend_name,index);
        free(command);
        return 1;
    }

    for (int i = 0; i < 5; i++)
    {
        command[i] = buf[i];
    }

    if (strcmp(command, "!list") == 0) //查询好友列表
    {
        printf("Client:%s want to list friends\n", client->username);
        int index = matchUsername(client);
        if (Database[index]->friendNUM <= 0)
        {
            char* error_tips = "Error unable to find friends list\n";
            bufferevent_write(client->bev, error_tips, strlen(error_tips));
        }
        else
        {
            for (int i = 0; i < Database[index]->friendNUM; i++)
            {
                bufferevent_write(client->bev, Database[index]->friends[i]->username,
                    strlen(Database[index]->friends[i]->username));
                char* status = Check_friend_status(find_user_index(Database[index]->friends[i]->username));
                bufferevent_write(client->bev, status, strlen(status));
                //bufferevent_write(client->bev, "\n", 1);
            }
        }

        free(command);
        return 1;
    }
    
    if (strcmp(command, "!delA") == 0) // 注销账号
    {
        printf("user: %s User wants to cancel account\n", client->username);
        int ok = delA(client);
        if (ok)
        {
            
            printf("user: %s Account deleted.\n", client->username);
            bufferevent_free(client->bev);
            return 1;
        }
        else
        {
            bufferevent_write(client->bev, "Failed to delete account.\n", 27);
        }
    }

    if (strcmp(command, "!help") == 0)
    {
        Help_documentation(client);
        return 1;
    }

    if (buf[0] == '!')
    {
        int index = find_user_index(client->username);
        return sendMessage(client, buf, index);
    }

    return 0;
}

int validate_password(int index, const char* password)
{
    if (index < 0 || index >= data_flog) return 0;
    if (strcmp(Database[index]->password, password) == 0 && Database[index]->state == STATE_OFFLINE)
    {
        return 1;
    }
    if (Database[index]->state == STATE_ONLINE)
        printf("The account: %s has been logged in elsewhere\n",Database[index]->username);
    return 0;
    /*if (strcmp(Database[index]->password, password) == 0)
    {
        if (Database[index]->state == STATE_ONLINE)
        {
            printf("The account has been logged in elsewhere\n");
            return 0;
        }
        return 1;
    }*/
    //return strcmp(Database[index]->password, password) == 0;
}

int register_user(const char* name, const char* password) 
{
    if (data_flog >= MAX_CLIENT) 
    {
        return 0;  // 数据库已满
    }

    if (find_user_index(name) != -1) 
    {
        return 0; // 用户已存在
    }
    
    Client *new_user = (Client *)malloc(sizeof(Client));
    if (!new_user) return 0; 

    memset(new_user, 0, sizeof(Client));
    strncpy(new_user->username, name, sizeof(new_user->username) - 1);
    new_user->username[sizeof(new_user->username) - 1] = '\0';
    strncpy(new_user->password, password, sizeof(new_user->password) - 1);
    new_user->password[sizeof(new_user->password) - 1] = '\0';
    new_user->state = STATE_WAIT_LOGIN;
    new_user->bev = NULL;
    new_user->friendNUM = 0;
    for (int i = 0; i < MAX_FRIEND; i++) 
    {
        new_user->friends[i] = NULL;
    }

    Database[data_flog++] = new_user;  // 将新用户保存到数据库
    printf("New user registered: %s\n", new_user->username);
    return 1;
}

int check_user(char* name, char* password)
{
    //printf("name: %s password: %s\n", name, password);
    int index = find_user_index(name);
    if (index >= 0)
    {
        return validate_password(index, password);
    }
    else
    {
        return register_user(name, password);
    }
}


void trim_newline(char* str) 
{
    char *p = strchr(str, '\n');
    if (p) *p = '\0';
}

void read_cb(struct bufferevent *bev, void *arg)
{
    Client *client = (Client *)arg;
    char buf[1024] = {0};
    int n = bufferevent_read(bev, buf, sizeof(buf) - 1);
    if (n <= 0) return;
    //test
    /*printf("user_name: %s\n", client->username);*/
    trim_newline(buf);  // 清除换行符

    switch (client->state) 
    {

        case STATE_WAIT_LOGIN:

            strncpy(client->username, buf, sizeof(client->username) - 1);
            client->username[sizeof(client->username) - 1] = '\0';  // 保证以 null 结尾
            client->state = STATE_WAIT_PASSWORD;

            bufferevent_write(bev, "Enter password:\n", 16);
            break;

        case STATE_WAIT_PASSWORD:

            strncpy(client->password, buf, sizeof(client->password) - 1);
            client->password[sizeof(client->password) - 1] = '\0';

            if (check_user(client->username, client->password))
            {
                client->state = STATE_ONLINE;
                turnUserStateInServ(find_user_index(client->username), client->state);
                //bev改变
                Database[find_user_index(client->username)]->bev = client->bev;
                bufferevent_write(bev, "Account login successful, welcome to PYL_CHAT, hope you have a happy day!\n", 80);
                bufferevent_write(bev, "You can enter !help to list help documentation\n", 
                    strlen("You can enter !help to list help documentation\n"));
                //bufferevent_write(bev, Tips, sizeof(Tips));
            }
            else
            {
                bufferevent_write(bev, "Login failed, try again:\n", 25);
                bufferevent_write(bev, "Please enter your username:\n", 28);
                client->state = STATE_WAIT_LOGIN;
                memset(client->username, 0, sizeof(client->username));
                memset(client->password, 0, sizeof(client->password));
            }

            break;

        case STATE_ONLINE:
            if (judgmentCommand(buf, client) == 0)
            {
                char tip_error[] = "command error please input correct command\n";
                bufferevent_write(bev, tip_error, sizeof(tip_error));
                printf("Client: %s used incorrect command, the Client input: %s\n", client->username, buf);
            }

            break;

        case STATE_OFFLINE:
            break;
    }
}

void event_cb(struct bufferevent *bev, short events, void *arg)
{
    Client *client = (Client *)arg;

    if (events & BEV_EVENT_EOF)
    {
        printf("Client: %s connection closed\n",client->username);
        
        //bev改变
        if (find_user_index(client->username) != -1)
        {
            turnUserStateInServ(find_user_index(client->username), STATE_OFFLINE);
            Database[find_user_index(client->username)]->bev = NULL;
        }
        
    }
    else if(events & BEV_EVENT_ERROR)
    {
        printf("some other error\n");
    }

    // 关闭客户端连接并释放资源
    bufferevent_free(bev);
    // 释放该客户端占用的内存
    free(client);
    printf("buffevent 资源已经被释放...\n");
}

// 回调函数：处理连接请求
void cb_listener(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *addr, int len, void *ptr)
{
    // 创建 bufferevent
    struct event_base* base = ptr;
    struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);

    // 创建客户端对象，存储状态等信息
    Client *client = (Client *)malloc(sizeof(Client));
    memset(client, 0, sizeof(Client));
    client->bev = bev;
    client->state = STATE_WAIT_LOGIN; // 初始为等待登录状态

    // 绑定回调（读写事件、错误事件）
    bufferevent_setcb(bev, read_cb, NULL, event_cb, client);
    bufferevent_enable(bev, EV_READ | EV_WRITE);

    // 初始欢迎提示
    const char *msg = "Welcome! Please enter your username:\n";
    bufferevent_write(bev, msg, strlen(msg));
}

// 初始化 socket 地址
struct sockaddr_in init_Sockaddr(uint16_t port) 
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    return addr;
}

int main(int argc, char* argv[])
{
    struct sockaddr_in serv_addr =  init_Sockaddr(SERV_PORT);
    
    struct event_base* base = event_base_new();
    
    struct evconnlistener* listener;
    listener = evconnlistener_new_bind(base, cb_listener, base,
            LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
            1024,
            (struct sockaddr*)&serv_addr,
            sizeof(serv_addr));

    setbuf(stdout, NULL);  // 禁用 stdout 缓冲，printf 会立即输出(不然linux的重定向符不能使用)

    event_base_dispatch(base);

    // 释放资源
    evconnlistener_free(listener);
    event_base_free(base);
    return 0;
}

