#!/bin/bash
#this is a bash script to read logdata about TCP connection
#2006-4-18,lianlinjiang,BUAA-CSE
echo "read logdata about queue connection......"
touch queuedata_blue.txt
rm queuedata_blue.txt
touch queuedata_blue.txt
i=0
while [ "$i" -le "30000" ]
do
insmod /root/AQM/blue/queuedata/seqfile_queuedata_blue.ko queue_array_count=$i
cat /proc/data_seq_file >> queuedata_blue.txt
rmmod seqfile_queuedata_blue
i=$[$i + 40]
done
echo "read logdata succed! pelease see queuedata.txt"
