#! /bin/bash

sudo rmmod es9039q2m-machine
sudo rmmod es9039q2m-i2c

sleep 0.5

sudo modprobe es9039q2m-i2c
sudo modprobe es9039q2m-machine
