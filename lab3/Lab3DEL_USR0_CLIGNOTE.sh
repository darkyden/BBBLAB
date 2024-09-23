#!/bin/bash
. del_path.sh

echo timer > $DEL_PATH"0/trigger"
echo 300 > $DEL_PATH"0/delay_on"
echo 700 > $DEL_PATH"0/delay_off"
