#!/bin/sh

echo "Starting attack agent"
./out/linux-x64-lock-no-ble/chip-lock-app \
  --secured-device-port 5541 \
  --unsecured-commissioner-port 5551 \
  --KVS /tmp/kvs-1 \
  --app-pipe /tmp/fifo-1 >/tmp/log-1 2>&1 &

sleep 1

echo "Commissioning attack agent"
./out/linux-x64-chip-tool/chip-tool \
    pairing code 0x69690001 MT:-24J0AFN00KA0648G00 >/dev/null 2>&1

echo "Starting target"
./out/linux-x64-light-no-ble/chip-lighting-app \
  --secured-device-port 5542 \
  --unsecured-commissioner-port 5552 \
  --KVS /tmp/kvs-2 \
  --app-pipe /tmp/fifo-2 >/tmp/log-2 2>&1 &

sleep 1

echo "Commissioning target"
./out/linux-x64-chip-tool/chip-tool \
    pairing code 0x69690002 MT:-24J0AFN00KA0648G00 >/dev/null 2>&1

echo "Starting pentest"

CMD="{\"Cmd\": \"RunScan\", \"Params\": {\"EndpointId\": 1, \"NodeId\": \"0x69690001\"}}"
echo $CMD > /tmp/fifo-1

echo "Finished"

kill -9 $(pidof chip-lock-app)
kill -9 $(pidof chip-lighting-app)

rm -f /tmp/chip_* /tmp/kvs-* /tmp/fifo-*
