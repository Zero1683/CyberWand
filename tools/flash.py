"""Explicit flashing tool. Default only updates the application, preserving templates."""
import argparse,subprocess,sys
from pathlib import Path
from serial.tools import list_ports
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--port');p.add_argument('--list',action='store_true');p.add_argument('--full',action='store_true',help='Fresh-board bootloader/partitions/app installation');p.add_argument('--recovery',action='store_true');a=p.parse_args()
ports=[x.device for x in list_ports.comports() if (x.vid,x.pid)==(0x303A,0x1001)]
if a.list:print('\n'.join(ports) or 'No ESP32 native USB found');raise SystemExit()
port=a.port or (ports[0] if len(ports)==1 else None)
if not port:raise SystemExit('Select one native USB port with --port; use --list')
app=ROOT/'firmware/.pio/build/mozhang/firmware.bin'
if not app.exists():app=ROOT/'release/firmware.bin'
if a.recovery:app=ROOT/'release/recovery-v0.6.bin'
files=['0x10000',str(app)]
if a.full:
 if a.recovery:raise SystemExit('--full and --recovery cannot be combined')
 # Always use one matching release set for a fresh-board installation.
 files=['0x0',str(ROOT/'release/bootloader.bin'),'0x8000',str(ROOT/'release/partitions.bin'),'0xe000',str(ROOT/'release/boot_app0.bin'),'0x10000',str(ROOT/'release/firmware.bin')]
raise SystemExit(subprocess.call([sys.executable,'-m','esptool','--chip','esp32c3','--port',port,'--baud','115200','--before','default-reset','--after','hard-reset','write-flash',*files]))
