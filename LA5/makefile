CC = gcc
CFLAGS = -Wall

all: leader follower

leader: leader.c
	$(CC) $(CFLAGS) -o leader leader.c

follower: follower.c
	$(CC) $(CFLAGS) -o follower follower.c

ifdef ARG
rl: leader
	./leader $(ARG)

rf: follower
	./follower $(ARG)
else
rl: leader
	./leader

rf: follower
	./follower
endif

clean:
	rm -f leader follower l f