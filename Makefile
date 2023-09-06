CFLAGS = -g -Wall -Werror -pthread

all:ttts

ttts: ttts.c 
	gcc $(CFLAGS) ttts.c -o ttts

message: message.c
	gcc $(CFLAGS) message.c -o message

client: ttt.c
	gcc $(CFLAGS) ttt.c -o ttt

clean_t:
	rm ttts

clean_m:
	rm message
