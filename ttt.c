#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include "message.h"
#include "message.c"

#define h_addr h_addr_list[0]

char *curr_msg;

int sentDraw = 1;

void error(const char *msg)
{
    perror(msg);
    printf("Either the server shut down or the other player disconnected.\nGame over.\n");

    exit(0);
}

int handle_draw()
{

    // printf("accept or reject draw? : ");
    // we'll have to read input from the user
    char drawBuf;

    printf("Enter your response to draw: ");
    drawBuf = getchar();

    if (drawBuf == 'A')
    {
        return 1;
        // this means accept the draw
    }
    else if (drawBuf == 'R')
    {
        // this means reject
        return 0;
    }
    printf("Invalid Answer\n");
    // after this, we will send a message to the server in the form DRAW|A|, DRAW|R|, or DRAW|S|
    // draw S means suggesting a draw
    return -1;
}

int is_all_newlines(char *whatever)
{
    for (int i = 0; i < strlen(whatever); i++)
    {
        if (whatever[i] != '\n')
        {
            return 0;
        }
    }
    return 1;
}

int recv_msg(int sockfd, char board[9], int id)
{
    signal(SIGPIPE, SIG_IGN);
    int num, read_bytes, overflowed;

    // keep track of whether there was an overflow message
    overflowed = 0;

    // step 1: check if there's anything in the message buffer in the first place
    if (strlen(msg_buf) == 0)
    {
        read_bytes = read(sockfd, msg_buf, MSG_LEN);
        if (is_all_newlines(msg_buf))
        {
            bzero(msg_buf, MSG_LEN);
            read_bytes = read(sockfd, msg_buf, MSG_LEN);
        }

        num = identify_msg(read_bytes);
        // Server will terminate if there's any issue with the message
        if (num <= 0)
        {
            exit(num);
        }
    }
    else
    {
        read_bytes = strlen(msg_buf);
        num = identify_msg(read_bytes);
        if (num <= 0)
        {
            exit(num);
        }
    }

    while (num != PROPER_MSG && (strlen(msg_buf) != 20))
    {
        if (num == MISSING_INFO)
        {
            int addl_bytes = read(sockfd, &msg_buf[read_bytes], MSG_LEN - read_bytes);
            if (addl_bytes == 0 || addl_bytes == -1)
            {
                exit(addl_bytes);
            }
            read_bytes += addl_bytes;
            num = identify_msg(read_bytes);
        }
        if (num == OVERFLOW_MSG)
        {
            num = handle_overflow();
            overflowed = 1;
        }
        if (num <= 0)
        {
            exit(num);
        }
    }

    free(curr_msg);
    // now we should have a proper message either in msg_buf or first_msg depending on overflow
    if (overflowed == 1)
    {
        get_msg_tokens(first_msg);
        curr_msg = strdup(first_msg);
        free(first_msg);
    }
    else
    {
        get_msg_tokens(msg_buf);
        curr_msg = strdup(msg_buf);
        bzero(msg_buf, MSG_LEN);
    }

    char *cmd = msg_fields[0];

    if (strcmp(cmd, "MOVD") == 0)
    {
        board = msg_fields[3];
        for(int i = 0; i < 4; i++) {
            printf("%s|", msg_fields[i]);
        }
        printf("\n");
        // NOT YOUR TURN
        if ((strcmp(msg_fields[2], "X") == 0) && (id == 0))
        {
            return 4;
        }
        if ((strcmp(msg_fields[2], "O") == 0) && (id == 1))
        {
            return 4;
        }

        // sample MOVD message: MOVD|16|X|2,2|....X....|

        // YOUR TURN
        return 7;
    }
    else if ((strcmp(cmd, "BEGN") == 0) && strcmp(msg_fields[2], "X") == 0)
    {
        return 1;
    }
    else if ((strcmp(cmd, "BEGN") == 0) && strcmp(msg_fields[2], "O") == 0)
    {
        return 0;
    }
    else if (strcmp(cmd, "OVER") == 0)
    {
        return 5;
    }
    else if (strcmp(cmd, "INVL") == 0)
    {
        return 6;
    }
    else if (strcmp(cmd, "WAIT") == 0)
    {
        return 2;
    }
    else if (strcmp(cmd, "DRAW") == 0)
    {

        // if it's a rejection, return 8
        if (strcmp(msg_fields[2], "R") == 0)
        {
            if (sentDraw != 0)
            {
                return 8;
            }
            else
            {
                return 9;
            }
        }

        // if it's a suggestion, return 3
        return 3;
    }
    return -1;

    // message handlers:
    // -handle INVL = 6
    // -handle OVER = 5
    // -handle MOVD = 4 if not your turn, 7 if it is
    // -handle DRAW = 3
    // -hanlde WAIT = 2
    // -handle BEGN for X = 1
    // -handle BEGN for Y = 0
}

void draw_board(char board[9])
{
    for (int i = 0; i < 8; i++)
    {
        printf("%c", board[i]);
    }
    printf("\n");
}

int recv_int(int sockfd)
{
    int msg = 0;
    int n = read(sockfd, &msg, sizeof(int));

    if (n < 0 || n != sizeof(int))
        error("ERROR reading int from server socket");

    return msg;
}

void write_server_int(int sockfd, int msg)
{
    int n = write(sockfd, &msg, sizeof(int));
    if (n < 0)
    {
        error("ERROR writing int to server socket");
    }
}

int connect_to_server(char *hostname, int portno)
{
    struct sockaddr_in serv_addr;
    struct hostent *server;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
        error("ERROR opening socket for server.");

    server = gethostbyname(hostname);

    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    memmove(server->h_addr, &serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR connecting to server");

    return sockfd;
}

void take_turn(int sockfd)
{
    char buffer[256];

    while (1)
    {
        // Could I send over a length?
        fgets(buffer, 256, stdin);
        int bytes_sent = 0;
        while (bytes_sent < strlen(buffer))
        {
            bytes_sent += write(sockfd, buffer, strlen(buffer));
        }
        break;
    }
}

void get_update(int sockfd, char board[9])
{

    int player_id = recv_int(sockfd);
    int move = recv_int(sockfd);
    board[move] = player_id ? 'O' : 'X';
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
        exit(0);
    }

    int sockfd = connect_to_server(argv[1], atoi(argv[2]));

    int id = recv_int(sockfd);

    // ./a.out plastic.cs.rutgers.edu 14999
    char board[9] = ".........";

    printf("Tic-Tac-Toe\n------------\n");
    // message handlers:
    // -handle INVL = 6
    // -handle OVER = 5
    // -handle MOVD = 4 if not your turn, 7 if it is
    // -handle DRAW = 3
    // -hanlde WAIT = 2
    // -handle BEGN for X = 1
    // -handle BEGN for O = 0

    printf("Game on!\n");
    printf("Your are %c's\n", id ? 'O' : 'X');

    // O we're x, 1 we're y

    draw_board(board);
    int cmdMsg;
    printf("Enter play message: \n");
    take_turn(sockfd);
    bzero(msg_buf, MSG_LEN);

    // BEGN and MOVD need special treatment based on who's receiving it becuause both do
    // basically if MOVD's Player ID doesn't match our own
    // this is why we have the ID variable

    while (1)
    {
        cmdMsg = recv_msg(sockfd, board, id);

        if (cmdMsg == 1 || cmdMsg == 6 || cmdMsg == 7)
        {
            // if it's one (begn), that doesnt necessarily make it our turn
            // if we receive 1/BEGN, we should check if the message contains X or O
            // it's O, we should proceed like it's WAIT
            sentDraw = 1;
            printf("Your turn: \n");
            take_turn(sockfd);
        }
        else if (cmdMsg == 8)
        {
            printf("Your draw was rejected\n");
            printf("Your turn: \n");
            take_turn(sockfd);
        }
        else if (cmdMsg == 2 || cmdMsg == 0 || cmdMsg == 4)
        {
            sentDraw = 0;
            printf("Waiting for other players move...\n");
        }
        else if (cmdMsg == 3)
        {
            sentDraw = 0;
            printf("Draw requested, answer DRAW|2|A| or DRAW|2|R|: \n");
            take_turn(sockfd);
        }
        else if (cmdMsg == 9)
        {
            printf("Your rejected the draw\n");
        }
        else
            error("Unknown message.");
    }

    printf("Game over.\n");
    close(sockfd);
    return 0;
}