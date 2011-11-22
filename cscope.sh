find ${CSCOPE_DIRS} -type f \( -name '*.[chsS]' -o -name '*.cpp' -o -name '*.hpp' -o -name '*.hh' -o -name '*.asm' \) -print > cscope.files
cscope -k -q -b -f cscope.out

