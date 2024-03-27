#!/usr/bin/env bash

# usage: ./prepare_machine.sh <freq, kHz>

# Fix frequency.
FIX_FREQ=${1}

echo performance | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
echo ${FIX_FREQ} | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq
echo ${FIX_FREQ} | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq

# Verify frequency settings.
cat /proc/cpuinfo | grep MHz
