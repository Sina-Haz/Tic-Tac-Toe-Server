#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>

#define MSG_LEN 264
#define CMD_LEN 4
#define MAX_TOTAL_FIELDS 5
#define MALFORMED_MSG -5
#define PROPER_MSG 1
#define MISSING_INFO 2
#define OVERFLOW_MSG 3

extern char msg_buf[MSG_LEN];
extern char *msg_fields[MAX_TOTAL_FIELDS];
extern int curr_fields;

int getNumFields(char *cmd);

int get_msg_tokens();

char *extract_substr();

int identify_msg(int read_bytes);

int handle_overflow();

#endif