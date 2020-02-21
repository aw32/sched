#!/bin/bash

export TASKLIST="markov bfs"
export SIZELIST="128"

./test_all.sh >> test.log


export TASKLIST="correlation heat bfs markov gaussblur"
export SIZELIST="256
512
1024
2048"

./test_all.sh >> test.log

export TASKLIST="correlation heat bfs gaussblur"
export SIZELIST="4096"

./test_all.sh >> test.log
