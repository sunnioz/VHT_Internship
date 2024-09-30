#!/bin/bash

# Mở terminal cho gNodeBv2 và đặt tiêu đề là "gNodeBv2 Terminal"
gnome-terminal --title="gNodeBv2 Terminal" -- bash -c "./gNodeBv2; exec bash"

# Số lượng tiến trình UE cần chạy
num_ue=4

# Vòng lặp để mở các terminal cho UE0, UE1, UE2, UE3
for i in $(seq 0 $((num_ue - 1)))
do
    gnome-terminal --title="UE$i Terminal" -- bash -c "./UE $i; exec bash"
done
