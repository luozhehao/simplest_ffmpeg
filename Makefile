all:
	gcc simplest_ffmpeg_player_su.c -g -o simplest_ffmpeg_player_su.out -I /usr/local/include -L /usr/local/lib -lSDLmain -lSDL -lavformat -lavcodec -lavutil -lswscale