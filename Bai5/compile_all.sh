#!/bin/bash

gcc UE.cpp -o UE

# Compile UE0.cpp
gcc UE0.cpp -o UE0
echo "Compiled UE0.cpp -> UE0"

# Compile UE1.cpp
gcc UE1.cpp -o UE1
echo "Compiled UE1.cpp -> UE1"

# Compile UE2.cpp
gcc UE2.cpp -o UE2
echo "Compiled UE2.cpp -> UE2"

# Compile UE3.cpp
gcc UE3.cpp -o UE3
echo "Compiled UE3.cpp -> UE3"

gcc gNodeBv2.cpp -o gNodeBv2
echo "Compiled gNodeB"

gcc AMF.cpp -o AMF
echo "Compiled AMF"
