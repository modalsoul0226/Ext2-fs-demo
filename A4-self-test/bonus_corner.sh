cp ./A4-self-test/images/twolevel.img ./A4-self-test/runs/caseX-crazy.img

./ext2_dump ./A4-self-test/runs/caseX-crazy.img > ./A4-self-test/solution-results/caseX-crazy.txt
# Copy
echo "Mkdir Test 1"
./ext2_mkdir ./A4-self-test/runs/caseX-crazy.img /folder2

echo "Mkdir Test 1"
./ext2_mkdir ./A4-self-test/runs/caseX-crazy.img /folder2/lalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalala

echo "Mkdir Test 2"
./ext2_mkdir ./A4-self-test/runs/caseX-crazy.img /folder2/lalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalo

echo "Mkdir Test 3"
./ext2_mkdir ./A4-self-test/runs/caseX-crazy.img /folder2/lalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalale

# echo "Dump Before"
# ./ext2_dump ./A4-self-test/runs/caseX-crazy.img > caseX-before.txt

echo "Mkdir Test 4 Need new block for this directory"
./ext2_mkdir ./A4-self-test/runs/caseX-crazy.img /folder2/lalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalali

echo "Mkdir Test 5 Need new block for this directory"
./ext2_mkdir ./A4-self-test/runs/caseX-crazy.img /folder2/lalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalaly

# echo "Dump After"
# ./ext2_dump ./A4-self-test/runs/caseX-crazy.img > caseX-after.txt
echo "Mkdir Test 6"
./ext2_mkdir ./A4-self-test/runs/caseX-crazy.img /folder2/lalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalala/a

echo "Mkdir Test 7"
./ext2_mkdir ./A4-self-test/runs/caseX-crazy.img /folder2/lalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalala/b

echo "Mkdir Test 8"
./ext2_mkdir ./A4-self-test/runs/caseX-crazy.img /folder2/lalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalala/c

echo "Mkdir Test 9"
./ext2_mkdir ./A4-self-test/runs/caseX-crazy.img /folder2/lalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalala/d

echo "Mkdir Test 10"
./ext2_mkdir ./A4-self-test/runs/caseX-crazy.img /folder2/lalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalala/e

echo "Copy Test 11 Lots of files"
the_files="$(ls ./A4-self-test/files)"
for the_file in $the_files
do
	g=$(basename $the_file)
	./ext2_cp ./A4-self-test/runs/caseX-crazy.img ./A4-self-test/files/$g /folder2/lalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalalala/e/$g
done

echo "Copy Test 12 Lots of files"
./ext2_cp ./A4-self-test/runs/caseX-crazy.img ./A4-self-test/files/largefile.txt /folder2/largefile.txt

./ext2_dump ./A4-self-test/runs/caseX-crazy.img > ./A4-self-test/results/caseX-crazy-before.txt

echo "Remove Test 12 Remove folder2"
./ext2_rm_bonus ./A4-self-test/runs/caseX-crazy.img -r /folder2

./ext2_dump ./A4-self-test/runs/caseX-crazy.img > ./A4-self-test/results/caseX-crazy.txt

echo "Compare Test 13"
diff ./A4-self-test/results/caseX-crazy.txt ./A4-self-test/solution-results/caseX-crazy.txt