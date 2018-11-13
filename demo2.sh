# Copy
echo "--- Copy Test ---"
./ext2_cp A4-self-test/runs/case1-cp.img A4-self-test/files/oneblock.txt /file.txt

# echo "--- Remove Test ---"
# ./ext2_rm A4-self-test/runs/case1-cp.img /file.txt

# dump the result disk
./ext2_dump A4-self-test/runs/case1-cp.img