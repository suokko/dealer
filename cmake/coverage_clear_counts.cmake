# Clear files based on ${OBJS} list and ${SUFFIX} replacement file ending

list(TRANSFORM OBJS REPLACE "\\.[^.]+$" ".${SUFFIX}")

file(REMOVE ${OBJS})



