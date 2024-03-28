#!/usr/bin/env bash

# Script configure IAA devices
# Usage : ./configure_iaa_user <mode> <start,end> <wq_size> <engine_number>
# mode: 0 - shared, 1 - dedicated
# devices: device number OR start and end device number.
# For example, 1, 7 will configure all the Socket0 devices in host,
# 9, 15 will configure all the Socket1 devices and son on,
# 1 will configure only device 1
# wq_size: 1-128
# engine_number: 1-8 - number of engines per device

#
# Global configs.
#
# Max engines per device -- CHANGE IF REQUIRED.
max_engines=8
# Max WQs per device -- CHANGE IF REQUIRED.
max_wqs=8

#
# Count IAA instances in the system.
#
iax_dev_id="0cfe"
num_iax=$(lspci -d:${iax_dev_id} | wc -l)
echo "Found ${num_iax} IAX instances"

#
# Parse input.
#
dedicated=${1:-0}; shift
device_num=${1:-$num_iax}; shift
wq_size=${1:-128}; shift
engine_number=${1:-128}; shift

if [ ${dedicated} -eq 0 ]; then
 mode="shared"
else
 mode="dedicated"
fi

#
# Disable all IAA devices, engines, and WQs.
#
echo "Disable all IAA devices, engines, and WQs."

first=1 && step=2
for ((i = ${first}; i < ${step} * ${num_iax}; i += ${step})); do
 for ((j = 0; j < ${max_wqs}; j += 1)); do
  accel-config disable-wq iax${i}/wq${i}.${j}
 done

 echo disable iax iax${i}
 accel-config disable-device iax${i}
done

echo
echo "************"
echo "Enabling Accelerators"
echo "************"
echo

#
# Enable/Disable PRS (not supported in every system).
#
lspci -vvv | grep 0cfe | awk '{print $1}' | while IFS= read -r line; do
    device_address="00:${line}"
    sudo setpci -s "$device_address" 244.B=1 # Change to 1/0 Enable/Disable
done
echo "PRS enabled on all devices!"

#
# Enable IAA devices, engines, and WQs.
#
if [ ${device_num} == $num_iax ]; then
 echo "Configuring all devices"
 start=${first}
 end=$(( ${step} * ${num_iax} ))
else
 echo "Configuring devices ${device_num}"
 declare -a array=($(echo ${device_num}| tr "," " "))
 start=${array[0]}
 if [ ${array[1]} ];then
 end=$((${array[1]} + 1 ))
 else
 end=$((${array[0]} + 1 ))
 fi
fi

# Enable loop.
echo "Enable IAA devices: ${start} to ${end}"
for ((i = ${start}; i < ${end}; i += ${step})); do
 # Config engines.
 for ((j = 0; j < ${engine_number}; j += 1)); do
  echo enable engine iax${i}/engine${i}.${j}
  accel-config config-engine iax${i}/engine${i}.${j} --group-id=0
 done

 # Config all WQs (even if only 1 is used), otherwise the config will fail.
 for ((j = 0; j < ${max_wqs}; j += 1)); do
  # -g: group-id
  # -s: wq size
  # -p: wq priority
  # -m: mode, dedicated/shared
  # -n: name
  # -t: threashold
  # -x: max transfer size
  # -b: block on fault (not supported in some systems)
  # 
  # Basic config.
  accel-config config-wq iax${i}/wq${i}.${j} -g 0 -s $wq_size -p 10 -m ${mode} -y user -n user${i} -t $wq_size -d user
  
  # Extended config.
  # accel-config config-wq iax${i}/wq${i}.${j} -g 0 -s $wq_size -p 10 -m ${mode} -y user -n user${i} -t $wq_size -d user -x 1073741824 -b 1
 done

 # Enable device.
 echo enable device iax${i}
 accel-config enable-device iax${i}

 # Enable WQs, one for each engine.
 for ((j = 0; j < ${engine_number}; j += 1)); do
  accel-config enable-wq iax${i}/wq${i}.${j}
 done

done
