"""Receive MOZHANG BLE gestures. pip install bleak; python ble_receiver.py [device-name]."""
import asyncio,json,struct,sys
SERVICE='aa490001-31c7-4e70-b655-12c923ec1a01'
EVENT='aa490002-31c7-4e70-b655-12c923ec1a01'
LABELS=['left','right','up','down','circle','zigzag']+[f'custom{i}' for i in range(1,7)]
def decode(data):
    if len(data)!=16:raise ValueError('Expected 16-byte notification')
    version,label,seq,uptime,distance,margin,reserved=struct.unpack('<BBIIHHH',data)
    if version!=1 or label>=len(LABELS):raise ValueError('Unsupported event')
    return dict(v=version,gesture=LABELS[label],seq=seq,uptime_ms=uptime,distance=distance/1000,margin=margin/1000)
async def main():
    from bleak import BleakClient,BleakScanner
    wanted=sys.argv[1] if len(sys.argv)>1 else None
    while True:
        device=await BleakScanner.find_device_by_filter(lambda d,a: SERVICE in [u.lower() for u in a.service_uuids] and (not wanted or a.local_name==wanted),timeout=15)
        if device is None:print('No wand advertising; retrying',flush=True);continue
        disconnected=asyncio.Event()
        try:
            async with BleakClient(device,disconnected_callback=lambda _:disconnected.set()) as client:
                def received(_,data):
                    try:print(json.dumps(decode(data),ensure_ascii=False),flush=True)
                    except ValueError as error:print(error,file=sys.stderr)
                await client.start_notify(EVENT,received)
                print('Connected:',device.name,flush=True);await disconnected.wait()
        except Exception as error:print(type(error).__name__,str(error),file=sys.stderr);await asyncio.sleep(2)
if __name__=='__main__':
    try:asyncio.run(main())
    except KeyboardInterrupt:pass
