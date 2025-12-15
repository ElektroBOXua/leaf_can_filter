LIB1='../../libraries/bitE/'
LIB2='../../libraries/charge_counter/'
LIB3='../../libraries/iso_tp/'

gcc -I"$LIB1" -I"$LIB2" -I"$LIB3" log_player.c -Wall -Wextra -g -std=c89 -pedantic
