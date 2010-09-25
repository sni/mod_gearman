#!/bin/bash

WHO=`whoami`
for sem in `ipcs -m | awk '{ print $2 }' | grep -v status | grep -v ID | grep -v Memory | grep $WHO`; do
    ipcrm -m $sem
done

for sem in `ipcs -s | awk '{ print $2 }' | grep -v status | grep -v ID | grep -v Memory | grep $WHO`; do
    ipcrm -s $sem
done

ipcs -a
