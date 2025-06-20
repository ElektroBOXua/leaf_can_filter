MISRA_PATH="../../misra"

TARGET="../leaf_can_filter.h"

rm ./a* 2> /dev/null

cppcheck --dump --std=c89 ${TARGET}
python ${MISRA_PATH}/misra.py ${TARGET}.dump --rule-texts=${MISRA_PATH}/misra_c_2023__headlines_for_cppcheck.txt
rm ${TARGET}.ctu*
rm ${TARGET}.dump*

gcc -I.. -I../libraries/bite/ "leaf_can_filter.test.c" -Wall -Wextra -g -std=c89 -pedantic
./a
