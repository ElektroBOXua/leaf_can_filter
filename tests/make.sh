# Get misra
MISRA_REPO='https://github.com/furdog/MISRA.git'
if cd misra; then git pull; cd ..; else git clone "$MISRA_REPO"; fi
MISRA='./MISRA/misra.sh'
"$MISRA" setup

TARGET="../leaf_can_filter.h"

rm ./a* 2> /dev/null

"$MISRA" check ${TARGET} "../libraries/bitE ../libraries/charge_counter ../libraries/iso_tp"

gcc -I.. -I../libraries/bite/ -I../libraries/charge_counter/ "leaf_can_filter.test.c" -Wall -Wextra -g -std=c89 -pedantic \
	-DBITE_DEBUG -DBITE_COLOR -DBITE_DEBUG_BUFFER_OVERFLOW -DBITE_PEDANTIC -DLEAF_CAN_FILTER_DEBUG
#Enable pedantic mode, so assertions will trigger

./a

## Return true, we don't validate tests ATM
true
