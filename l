#!/bin/bash

rm ./demo

make

export LD_LIBRARY_PATH=./ 
./demo