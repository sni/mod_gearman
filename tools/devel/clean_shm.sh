#!/bin/bash

WHO=`whoami`
for sem in `ipcs -m | grep $WHO | awk '{ print $2 }' | grep -v status | grep -v ID | grep -v Memory`; do
    ipcrm -m $sem
done

for sem in `ipcs -s | grep $WHO | awk '{ print $2 }' | grep -v status | grep -v ID | grep -v Memory`; do
    ipcrm -s $sem
done

ipcs -a
