LIB1='../../libraries/bitE/'
LIB2='../../libraries/charge_counter/'

gcc -I"$LIB1" -I"$LIB2" log_player.c -Wall -Wextra -g -std=c89 -pedantic
