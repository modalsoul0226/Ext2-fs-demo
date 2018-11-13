all: ext2_mkdir ext2_cp ext2_ln ext2_rm ext2_restore ext2_checker ext2_rm_bonus ext2_restore_bonus

ext2_mkdir :  ext2_mkdir.o path.o utils.o ialloc.o
	gcc -Wall -g -std=gnu99 -o ext2_mkdir $^

ext2_cp :  ext2_cp.o path.o utils.o ialloc.o
	gcc -Wall -g -std=gnu99 -o ext2_cp $^

ext2_ln :  ext2_ln.o path.o utils.o ialloc.o
	gcc -Wall -g -std=gnu99 -o ext2_ln $^

ext2_rm :  ext2_rm.o path.o utils.o ialloc.o
	gcc -Wall -g -std=gnu99 -o ext2_rm $^

ext2_restore :  ext2_restore.o path.o utils.o ialloc.o
	gcc -Wall -g -std=gnu99 -o ext2_restore $^

ext2_checker :  ext2_checker.o path.o utils.o ialloc.o
	gcc -Wall -g -std=gnu99 -o ext2_checker $^

ext2_rm_bonus :  ext2_rm_bonus.o path.o utils.o ialloc.o
	gcc -Wall -g -std=gnu99 -o ext2_rm_bonus $^

ext2_restore_bonus :  ext2_restore_bonus.o path.o utils.o ialloc.o
	gcc -Wall -g -std=gnu99 -o ext2_restore_bonus $^

%.o : %.c ext2.h
	gcc -Wall -g -c $<

clean : 
	rm -f *.o ext2_mkdir ext2_cp ext2_ln ext2_rm ext2_restore ext2_checker ext2_rm_bonus ext2_restore_bonus path *~