#!/usr/bin/env bash
rm -rf A4-self-test/runs/*
rm -rf A4-self-test/results/*

#--- First, copy out the images ---
# Copy
cp example/img/backup/emptydisk.img A4-self-test/runs/case1-cp.img

# dump the empty disk
echo "--- Visualize an empty disk ---"
./ext2_dump A4-self-test/runs/case1-cp.img