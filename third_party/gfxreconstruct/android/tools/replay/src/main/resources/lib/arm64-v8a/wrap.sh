#!/system/bin/sh

capture_pm4_enabled=$(getprop debug.dive.replay.capture_pm4)

echo "capture_pm4_enabled is " $capture_pm4_enabled
if [[ "$capture_pm4_enabled" == "true" || "$capture_pm4_enabled" == "1" ]]; then
    echo "in the true branch\n"
    export LD_PRELOAD=/data/local/tmp/libwrap.so 
    "$@"
else
    echo "in the false branch\n"
   "$@"
fi

# exec $cmd

# CAPTURE_PM4=true "$@"