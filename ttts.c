#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include "message.h"
#include "message.c"

#define MAX_PLAYERS 252
#define GAME_PARTICIPANTS 2
#define BOARD_SIZE 9

int player_count = 0;
pthread_mutex_t mutexcount;
int movesPlayed = 0;
int lastIndex = 0;
char *curr_msg;

// initialize for new game
void setMovesToBeggining()
{
    movesPlayed = 0;
    lastIndex = 0;
}

void error(const char *msg)
{
    perror(msg);
    pthread_exit(NULL);
}

// function that returns a socket fd for a socket bound to specified port
int setup_socket(int portno)
{
    int sockfd;
    struct sockaddr_in serv_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        error("Couldn't create socket!");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR binding listener socket.");

    return sockfd;
}

void write_msg_to_player(int playerfd, char *msg)
{
    signal(SIGPIPE, SIG_IGN);
    int n = write(playerfd, msg, strlen(msg));
    if (n < 0)
        error("ERROR writing msg to client socket");
}

// this is a function for identifying what the command is (only recognizes commands players can send)
int getPlayerCommand(char *cmd)
{
    printf("Made it to getPlayer command, cmd was %s\n", cmd);
    int num = -1;
    if (strcmp(cmd, "PLAY") == 0)
    {
        num = 1;
    }
    else if (strcmp(cmd, "MOVE") == 0)
    {
        num = 2;
    }
    else if (strcmp(cmd, "RSGN") == 0)
    {
        num = 3;
    }
    else if (strcmp(cmd, "DRAW") == 0)
    {
        num = 4;
    }
    return num;
}

// returns 1 if only has newline, otherwise returns a 0
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

// In this function we are:
// 1) Reading a full message. If a malformed or errorful message is sent we will terminate.
//  If missing info sent we keep reading until we get a full msg.
//  If the msg is overflowing we will only use 1st message and hold the rest of the message in the buffer
// numbers: -1 -> invalid command, 1 -> PLAY, 2 -> MOVE, 3 -> RSGN, 4 -> DRAW
int recv_msg(int cli_sockfd)
{
    signal(SIGPIPE, SIG_IGN);
    int num, read_bytes, overflowed;

    // keep track of whether there was an overflow message
    overflowed = 0;

    // step 1: check if there's anything in the message buffer in the first place
    if (strlen(msg_buf) == 0 || strlen(msg_buf) == 1) // because it was reading length 1 when it was empty
    {
        read_bytes = read(cli_sockfd, msg_buf, MSG_LEN);
        if (is_all_newlines(msg_buf))
        {
            bzero(msg_buf, MSG_LEN);
            read_bytes = read(cli_sockfd, msg_buf, MSG_LEN);
        }
        // printf("BREAK 2 and BUFF has %s\n", msg_buf);
        num = identify_msg(read_bytes);
        // Server will terminate if there's any issue with the message
        if (num <= 0)
        {
            exit(num);
        }
    }
    else
    {
        // printf("BUFFER ISN't EMPTY, IT HAS LENGTH %ld\n", strlen(msg_buf));
        read_bytes = strlen(msg_buf);
        num = identify_msg(read_bytes);
        if (num <= 0)
        {
            exit(num);
        }
    }

    printf("Received message was %s\n", msg_buf);

    while (num != PROPER_MSG)
    {
        if (num == MISSING_INFO)
        {
            int addl_bytes = read(cli_sockfd, &msg_buf[read_bytes], MSG_LEN - read_bytes);
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

    return getPlayerCommand(cmd);
}

void draw_board(char board[9])
{
    for (int i = 0; i < 8; i++)
    {
        printf("%c", board[i]);
    }
    printf("\n");
}

// This function setups listeners for the amount of players and accepts a batch of two new players
void get_players(int socket, int *player_fd)
{

    socklen_t clilen;
    struct sockaddr_in cli_addr;

    int num_connections = 0;
    while (num_connections < GAME_PARTICIPANTS)
    {
        // setup a listener to listen at that socket for number connections which is MAX_CONNECTIONS - curr_players
        if (listen(socket, MAX_PLAYERS + 1 - player_count) != 0)
        {
            error("Listen error");
        }
        memset(&cli_addr, 0, sizeof(cli_addr));
        clilen = sizeof(cli_addr);

        player_fd[num_connections] = accept(socket, (struct sockaddr *)&cli_addr, &clilen);

        if (player_fd[num_connections] < 0)
            error("ERROR accepting a connection from a client.");

        // give player an ID so they know if they are player 0 or 1
        write(player_fd[num_connections], &num_connections, sizeof(int));

        // increment the number of players on the server
        pthread_mutex_lock(&mutexcount);
        player_count++;
        printf("Number of players is now %d.\n", player_count);
        pthread_mutex_unlock(&mutexcount);

        // if (num_connections == 0)
        // {
        //     write_msg_to_player(player_fd[num_connections], "wait for other player to join.");
        // }

        num_connections++;
    }
}

// this checks if we won
int check_board(char board[9])
{
    // return 1 if won, 0 if not

    if ((board[0] == 'X' && board[1] == 'X' && board[2] == 'X') || (board[0] == 'O' && board[1] == 'O' && board[2] == 'O'))
    {
        return 1;
    }
    if ((board[3] == 'X' && board[4] == 'X' && board[5] == 'X') || (board[3] == 'O' && board[4] == 'O' && board[5] == 'O'))
    {
        return 1;
    }
    if ((board[6] == 'X' && board[7] == 'X' && board[8] == 'X') || (board[6] == 'O' && board[7] == 'O' && board[8] == 'O'))
    {
        return 1;
    }
    if ((board[0] == 'X' && board[3] == 'X' && board[6] == 'X') || (board[0] == 'O' && board[3] == 'O' && board[6] == 'O'))
    {
        return 1;
    }
    if ((board[1] == 'X' && board[4] == 'X' && board[7] == 'X') || (board[1] == 'O' && board[4] == 'O' && board[7] == 'O'))
    {
        return 1;
    }
    if ((board[2] == 'X' && board[5] == 'X' && board[8] == 'X') || (board[2] == 'O' && board[5] == 'O' && board[8] == 'O'))
    {
        return 1;
    }
    if ((board[0] == 'X' && board[4] == 'X' && board[8] == 'X') || (board[0] == 'O' && board[4] == 'O' && board[8] == 'O'))
    {
        return 1;
    }
    if ((board[6] == 'X' && board[4] == 'X' && board[2] == 'X') || (board[6] == 'O' && board[4] == 'O' && board[2] == 'O'))
    {
        return 1;
    }

    return 0;
}

// convert "num, num" to a int product
int parse_move(char *input_string)
{
    char *copy_string = strdup(input_string); // make a copy of the input string
    char *token;
    int values[2];
    int i = 0;

    token = strtok(copy_string, ",");
    while (token != NULL && i < 2)
    {
        values[i++] = atoi(token);
        token = strtok(NULL, ",");
    }

    free(copy_string); // free the copy of the input string

    if (i != 2)
    {
        // input string does not contain two comma-separated integers
        return -1;
    }
    if (values[0] < 0 || values[1] < 0 || values[0] > 3 || values[0] > 3)
    {
        return -1;
    }

    int x = values[0];
    int y = values[1];

    if (x == 1 && y == 1)
    {
        return 0;
    }
    else if (x == 2 && y == 1)
    {
        return 1;
    }
    else if (x == 3 && y == 1)
    {
        return 2;
    }
    else if (x == 1 && y == 2)
    {
        return 3;
    }
    else if (x == 2 && y == 2)
    {
        return 4;
    }
    else if (x == 3 && y == 2)
    {
        return 5;
    }
    else if (x == 1 && y == 3)
    {
        return 6;
    }
    else if (x == 2 && y == 3)
    {
        return 7;
    }
    else if (x == 3 && y == 3)
    {
        return 8;
    }

    return -1; // invalid coordiantes
}

// helper method to write a BEGN msg to the player. id = 0 -> player uses X otherwise use O.
void write_begn_msg(int playerfd, char *player_name, int id)
{
    char *msg = malloc(sizeof(char) * MSG_LEN);
    bzero(msg, MSG_LEN);

    strcat(msg, "BEGN|");

    char num_str[3];
    sprintf(num_str, "%lu", strlen(player_name) + 3);
    strcat(msg, num_str);
    strcat(msg, "|");

    if (id == 0)
    {
        strcat(msg, "X|");
    }
    else if (id == 1)
    {
        strcat(msg, "O|");
    }

    strcat(msg, player_name);
    strcat(msg, "|");

    write_msg_to_player(playerfd, msg);
    free(msg);
}

// helper method for sending OVER message. outcome is W, L, or D
void write_over_msg(int playerfd, char *outcome, char *reason)
{
    char *msg = malloc(sizeof(char) * MSG_LEN);
    bzero(msg, MSG_LEN);

    strcat(msg, "OVER|");
    char num_str[3];
    sprintf(num_str, "%lu", strlen(reason) + 3);
    strcat(msg, num_str);
    strcat(msg, "|");
    strcat(msg, outcome);
    strcat(msg, "|");
    strcat(msg, reason);
    strcat(msg, "|");

    write_msg_to_player(playerfd, msg);
    free(msg);
}

// lets say we get a draw request. first we send a draw to opp. If they send
// if outcome is 0 that means draw was rejected. if 1 it means it was accepted.
int handle_draw(int this_fd, int opp_fd)
{
    int outcome = 0;
    write_msg_to_player(opp_fd, "DRAW|2|S|");

    int cmd_int = recv_msg(opp_fd);
    if (cmd_int != 4)
        error("Player didn't address draw suggestion. Game terminated.");

    if (strcmp(curr_msg, "DRAW|2|A|") == 0)
    {
        write_over_msg(this_fd, "D", "Opponent accepted draw.");
        write_over_msg(opp_fd, "D", "You accepted the draw.");
        outcome = 1;
    }
    else if (strcmp(curr_msg, "DRAW|2|R|") == 0)
    {
        write_msg_to_player(this_fd, "DRAW|2|R|");
        write_msg_to_player(opp_fd, "DRAW|2|R|");
    }
    else
        error("Incorrectly formatted draw message. Game terminated.");

    return outcome;
}

void write_movd(int *player_fd, int id, char *board)
{
    char *msg = malloc(sizeof(char) * MSG_LEN);
    bzero(msg, MSG_LEN);

    strcat(msg, "MOVD|16|");
    if (id == 0)
    {
        strcat(msg, "X");
    }
    else if (id == 1)
    {
        strcat(msg, "O");
    }
    strcat(msg, "|");
    strcat(msg, board);
    strcat(msg, "|");

    write_msg_to_player(player_fd[0], msg);
    write_msg_to_player(player_fd[1], msg);
    free(msg);
}

void write_invl(int playerfd, char *reason)
{
    char *msg = malloc(sizeof(char) * MSG_LEN);
    bzero(msg, MSG_LEN);

    strcat(msg, "INVL|");
    char num_str[3];
    sprintf(num_str, "%lu", strlen(reason) + 3);
    strcat(msg, num_str);
    strcat(msg, "|");
    strcat(msg, reason);
    strcat(msg, "|");

    write_msg_to_player(playerfd, msg);
    free(msg);
}

// this is the function that the threads will run. Parameter thread_data will be the two player fds for the game
void *run_game(void *thread_data)
{
    int *player_fd = (int *)thread_data;
    char *player_names[GAME_PARTICIPANTS];
    char board[BOARD_SIZE] = ".........";

    printf("Starting Game!\n");

    bzero(msg_buf, MSG_LEN);
    // now we wait for first player to send PLAY message
    if (recv_msg(player_fd[0]) != 1)
        error("Player 1 didn't start game properly.");

    player_names[0] = strdup(msg_fields[2]);

    printf("Player 1 Name: %s\n", msg_fields[2]);

    write_msg_to_player(player_fd[0], "WAIT|0|");
    printf("Sent message\n");

    if (recv_msg(player_fd[1]) != 1)
        error("Player 2 didn't start game properly");

    player_names[1] = strdup(msg_fields[2]);

    write_msg_to_player(player_fd[1], "WAIT|0|");

    // write begin messages to players
    write_begn_msg(player_fd[0], player_names[0], 0);
    write_begn_msg(player_fd[1], player_names[1], 1);

    int prev_player_turn = 1;
    int player_turn = 0;
    int game_over = 0;
    int turn_count = 0;
    while (!game_over)
    {

        int valid = 0;
        while (!valid)
        {
            int cmd_int = recv_msg(player_fd[player_turn]);
            if (cmd_int < 2)
                error("player didn't send a valid message. game terminated.");

            // handle a RSGN command
            else if (cmd_int == 3)
            {
                write_over_msg(player_fd[player_turn], "L", "You resigned.");
                write_over_msg(player_fd[prev_player_turn], "W", "Opponent resigned.");
                valid = 1;
                game_over = 1;
            }

            // handle DRAW command
            else if (cmd_int == 4)
            {
                game_over = handle_draw(player_fd[player_turn], player_fd[prev_player_turn]);
                valid = 1;
                turn_count--;
                // do these two an extra time to offset the repeated move?
                prev_player_turn = player_turn;
                player_turn = (player_turn + 1) % 2;
            }

            // handle MOVE command
            else if (cmd_int == 2 || cmd_int == 1) // REMOVE THE == 1 PARTTTT
            {
                int pos = parse_move(msg_fields[3]);
                if (pos == -1)
                    error("MOVE was improperly formatted.");

                if (board[pos] == '.')
                {
                    valid = 1;
                    if (player_turn == 0)
                    {
                        board[pos] = 'X';
                    }
                    else
                    {
                        board[pos] = 'O';
                    }
                    write_movd(player_fd, player_turn, board);
                }
                else
                {
                    // stops at "write invalid"
                    write_invl(player_fd[player_turn], "That space is occupied.");

                    // //---------DELETE IF THERE's AN ERROR--------------
                    // turn_count--;
                    // // do these two an extra time to offset the repeated move?
                    // prev_player_turn = player_turn;
                    // player_turn = (player_turn + 1) % 2;
                    // //--------------------------------------------------
                }
            }
        }
        draw_board(board);

        game_over = check_board(board);

        if (game_over == 1)
        {
            write_over_msg(player_fd[player_turn], "W", "You got 3 in a row.");
            write_over_msg(player_fd[prev_player_turn], "L", "Opponent got 3 in a row.");
            printf("Player %d won.\n", player_turn);
        }
        else if (turn_count == 8)
        {
            printf("Draw.\n");
            write_over_msg(player_fd[player_turn], "D", "Board is full");
            write_over_msg(player_fd[prev_player_turn], "D", "Board is full");
            game_over = 1;
        }

        prev_player_turn = player_turn;
        player_turn = (player_turn + 1) % 2;
        turn_count++;
    }

    printf("Game over.\n");

    close(player_fd[0]);
    close(player_fd[1]);

    pthread_mutex_lock(&mutexcount);
    player_count--;
    printf("Number of players is now %d.", player_count);
    player_count--;
    printf("Number of players is now %d.", player_count);
    pthread_mutex_unlock(&mutexcount);

    free(player_fd);

    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    setMovesToBeggining();

    if (argc < 2)
    {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    int listen_sockfd = setup_socket(atoi(argv[1]));

    // using pthread_mutex_init we create a mutex object with default attributes.
    // mutexes are sync objects we use so that certain shared thread resources can be accessed only 1 at a time
    pthread_mutex_init(&mutexcount, NULL);

    while (1)
    {
        if (player_count <= MAX_PLAYERS)
        {
            // setup the fds from our 2 players.
            int *player_fd = malloc(sizeof(int) * GAME_PARTICIPANTS);
            memset(player_fd, 0, GAME_PARTICIPANTS * sizeof(int));

            // setup listener and accept a batch of two players.
            get_players(listen_sockfd, player_fd);

            // create a thread that will run the game for these two players.
            pthread_t thread;
            int result = pthread_create(&thread, NULL, run_game, (void *)player_fd);
            if (result)
            {
                printf("Thread creation failed with return code %d\n", result);
                exit(-1);
            }
        }
    }

    close(listen_sockfd);
    pthread_mutex_destroy(&mutexcount);
    pthread_exit(NULL);
}
