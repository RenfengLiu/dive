adb root
adb wait-for-device
adb push install/libwrap.so /data/local/tmp/libwrap.so
adb shell mkdir -p /data/local/debug/vulkan/
adb push  install/libVkLayer_dive.so /data/local/debug/vulkan/
adb shell setprop debug.vulkan.layers VK_LAYER_Dive
adb shell stop
adb shell start
adb wait-for-device
adb forward tcp:19999 tcp:19999
sleep 3
./build/bin/client_cli 