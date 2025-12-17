switchbot: switchbot.c
	cc -O3 -Wall -o switchbot switchbot.c

clean:
	rm -f switchbot
