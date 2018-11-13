#!/usr/bin/env bash
rm -rf A4-self-test/runs/*
rm -rf A4-self-test/results/*

#--- First, copy out the images ---

# Copy
cp example/img/backup/emptydisk.img A4-self-test/runs/case1-cp.img
cp example/img/backup/emptydisk.img A4-self-test/runs/case2-cp-large.img
cp A4-self-test/images/removed.img A4-self-test/runs/case3-cp-rm-dir.img

# Mkdir
cp example/img/backup/emptydisk.img A4-self-test/runs/case4-mkdir.img
cp example/img/backup/onedirectory.img A4-self-test/runs/case5-mkdir-2.img

# Link
cp example/img/backup/twolevel.img A4-self-test/runs/case6-ln-hard.img
cp example/img/backup/twolevel.img A4-self-test/runs/case7-ln-soft.img

# Remove
cp A4-self-test/images/manyfiles.img A4-self-test/runs/case8-rm.img
cp A4-self-test/images/manyfiles.img A4-self-test/runs/case9-rm-2.img
cp A4-self-test/images/manyfiles.img A4-self-test/runs/case10-rm-3.img
cp example/img/backup/largefile.img A4-self-test/runs/case11-rm-large.img

# Restore
cp A4-self-test/images/removed.img A4-self-test/runs/case12-rs.img
cp A4-self-test/images/removed.img A4-self-test/runs/case13-rs-2.img
cp A4-self-test/images/removed-largefile.img A4-self-test/runs/case14-rs-large.img

# # Checker
cp example/img/backup/twolevel-corrupt.img A4-self-test/runs/case15-checker.img

#--- Now, do the test cases ---

# Copy
echo "Copy Test 1"
./ext2_cp A4-self-test/runs/case1-cp.img A4-self-test/files/oneblock.txt /file.txt
echo "Copy Test 2"
./ext2_cp A4-self-test/runs/case2-cp-large.img A4-self-test/files/largefile.txt /big.txt
echo "Copy Test 3"
./ext2_cp A4-self-test/runs/case3-cp-rm-dir.img A4-self-test/files/oneblock.txt /level1/file

# Mkdir
echo "Mkdir Test 4"
./ext2_mkdir A4-self-test/runs/case4-mkdir.img /level1/
echo "Mkdir Test 5"
./ext2_mkdir A4-self-test/runs/case5-mkdir-2.img /level1/level2

# Link
echo "Link Test 6"
./ext2_ln A4-self-test/runs/case6-ln-hard.img /level1/level2/bfile /bfilelink
echo "Link test 7"
./ext2_ln A4-self-test/runs/case7-ln-soft.img -s /level1/level2/bfile /bfilesoftlink

# Remove
echo "Remove Test 8"
./ext2_rm A4-self-test/runs/case8-rm.img /c.txt
echo "Remove Test 9"
./ext2_rm A4-self-test/runs/case9-rm-2.img /level1/d.txt
echo "Remove Test 10"
./ext2_rm A4-self-test/runs/case10-rm-3.img /level1/e.txt
echo "Remove Test 11"
./ext2_rm A4-self-test/runs/case11-rm-large.img /largefile.txt

# Restore
#echo "Restore Test 12"
#./ext2_restore_bonus A4-self-test/runs/case12-rs.img -r /c.txt
#echo "Restore Test 13"
#./ext2_restore_bonus A4-self-test/runs/case13-rs-2.img -r /level1/e.txt
#echo "Restore Test 14"
#./ext2_restore_bonus A4-self-test/runs/case14-rs-large.img -r /largefile.txt
./ext2_restore A4-self-test/runs/case12-rs.img /c.txt
echo "Restore Test 13"
./ext2_restore A4-self-test/runs/case13-rs-2.img /level1/e.txt
echo "Restore Test 14"
./ext2_restore A4-self-test/runs/case14-rs-large.img /largefile.txt

# Checker
echo "Checker Test 15"
./ext2_checker A4-self-test/runs/case15-checker.img

# --- Now do the dumps ---
the_files="$(ls A4-self-test/runs)"
for the_file in $the_files
do
	g=$(basename $the_file)
	./ext2_dump A4-self-test/runs/$g > A4-self-test/results/$g.txt
	diff A4-self-test/solution-results/$g.txt A4-self-test/results/$g.txt
done


