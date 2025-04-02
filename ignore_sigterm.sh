#!/bin/bash

# Trap SIGTERM and do nothing (ignore it)
trap '' SIGTERM

echo "Process started. Try sending SIGTERM!"
while true; do
    sleep 1
done
