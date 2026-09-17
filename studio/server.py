"""Loopback-only, offline GUI. One serial worker owns the UART; bounded state."""
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import hashlib, datetime, json, re, secrets, threading, time, subprocess, os, sys
from urllib.parse import urlparse
import serial
from serial.tools import list_ports

ROOT=Path(__file__).resolve().parent
TOKEN_PATH=ROOT/'.local-token'
TOKEN=TOKEN_PATH.read_text(encoding='utf8').strip() if TOKEN_PATH.exists() else secrets.token_urlsafe(24)
if not TOKEN_PATH.exists():TOKEN_PATH.write_text(TOKEN,encoding='utf8')
LABELS=['left','right','up','down','circle','zigzag']+[f'custom{i}' for i in range(1,7)]
class Device:
    def __init__(self):
        self.lock=threading.RLock();self.serial=None;self.port='';self.error='';self.lines=deque(maxlen=100)
        self.motion=deque(maxlen=260);self.history=deque(maxlen=100);self.templates={k:0 for k in LABELS}
        self.health={};self.ready=False;self.capturing=False;self.mode='recognize';self.version='';self.last_seen=0
        self.sequence=0;self.commands=deque(maxlen=32);self.stop=None;self.thread=None;self.log=None
        self.dump=[];self.last_dump=[];self.expected=None;self.capture_expected=None;self.capture_mode='recognize'
        self.flash={'running':False,'message':''};self.names={};self.link={}
    def disconnect(self):
        if self.stop:self.stop.set()
        if self.thread:self.thread.join(timeout=2)
        with self.lock:
            if self.serial:self.serial.close()
            if self.log:self.log.close()
            self.serial=None;self.log=None;self.ready=False;self.capturing=False;self.commands.clear()
    def connect(self,port):
        if self.flash['running']:raise ValueError('正在烧录，请等待完成')
        ports={p.device:p for p in list_ports.comports()}
        if port not in ports or (ports[port].vid,ports[port].pid)!=(0x303A,0x1001):raise ValueError('请选择 ESP32 原生 USB 串口')
        self.disconnect()
        s=serial.Serial();s.port=port;s.baudrate=115200;s.timeout=.05;s.write_timeout=.5;s.dtr=True;s.rts=False
        s.open()
        with self.lock:
            self.serial=s;self.port=port;self.error='';self.last_seen=0;self.ready=False;self.health={};self.version='';self.motion.clear();self.templates={k:0 for k in LABELS}
            logs=ROOT.parent/'logs';logs.mkdir(exist_ok=True)
            self.log=(logs/(datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'-gui.log')).open('a',encoding='utf8',buffering=1)
            self.stop=threading.Event();self.commands.extend(['status','stream on'])
            self.thread=threading.Thread(target=self.worker,daemon=True);self.thread.start()
    def send(self,cmd):
        allowed={'status','calibrate','recognize','dump','stream on','stream off'}|{f'{op} {k}' for op in ['train','record','clear'] for k in LABELS}
        if cmd not in allowed:raise ValueError('不支持的命令')
        self.enqueue(cmd)
    def enqueue(self,cmd):
        with self.lock:
            if not self.serial or not self.serial.is_open:raise ValueError('请先连接设备')
            if len(self.commands)>=32:raise ValueError('命令队列已满')
            self.commands.append(cmd)
    def reconnect_after_settings(self):
        port=self.port
        def work():
            time.sleep(2)
            for _ in range(12):
                try:self.connect(port);return
                except (ValueError,OSError,serial.SerialException):time.sleep(1)
            with self.lock:self.error='重启后未找到原生 USB，请重新连接设备'
        threading.Thread(target=work,daemon=True).start()
    def flash_firmware(self,port):
        ports={p.device:p for p in list_ports.comports()}
        if port not in ports or (ports[port].vid,ports[port].pid)!=(0x303A,0x1001):raise ValueError('请选择 ESP32 原生 USB 串口')
        firmware=ROOT.parent/'firmware/.pio/build/mozhang/firmware.bin'
        if not firmware.is_file():firmware=ROOT.parent/'release/firmware.bin'
        if not firmware.is_file():raise ValueError('没有已构建的固件，请先构建')
        with self.lock:
            if self.flash['running']:raise ValueError('已经在烧录')
            self.flash={'running':True,'message':'正在释放串口并烧录…'}
        def work():
            try:
                self.disconnect()
                env=os.environ.copy();tmp=ROOT.parent/'.cache/tmp';tmp.mkdir(parents=True,exist_ok=True);env['TEMP']=env['TMP']=str(tmp)
                result=subprocess.run([sys.executable,'-X','utf8','-m','esptool','--chip','esp32c3','--port',port,'--before','default-reset','--after','hard-reset','write-flash','0x10000',str(firmware)],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=90,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
                logs=ROOT.parent/'logs';logs.mkdir(exist_ok=True)
                (logs/'last-usb-flash.log').write_text(result.stdout+result.stderr,encoding='utf8')
                if result.returncode:raise ValueError('烧录失败，请查看 logs/last-usb-flash.log')
                time.sleep(2)
                with self.lock:self.flash={'running':False,'message':'烧录完成，校验通过'}
                self.connect(port)
            except Exception as e:
                with self.lock:self.flash={'running':False,'message':str(e)}
        threading.Thread(target=work,daemon=True).start()
    def worker(self):
        pending=b''
        try:
            while not self.stop.is_set():
                with self.lock:cmd=self.commands.popleft() if self.commands else None
                if cmd:self.serial.write((cmd+'\n').encode());self.log.write('HOST '+('link [redacted]' if cmd.startswith('link ') else cmd)+'\n')
                data=self.serial.read(min(4096,self.serial.in_waiting or 1));pending+=data
                if len(pending)>16384:pending=b''
                while b'\n' in pending:
                    raw,pending=pending.split(b'\n',1);line=raw.decode('utf8','replace').strip()
                    self.log.write(line+'\n')
                    with self.lock:self.parse(line)
        except (serial.SerialException,OSError) as e:
            with self.lock:self.error=str(e);self.ready=False
            if self.serial:self.serial.close()
    def parse(self,line):
        self.last_seen=time.time()
        if line.startswith('MOTION '):
            try:
                values=[float(x) for x in line[7:].split(',')]
                if len(values)==7:self.motion.append(values)
            except ValueError:pass
            return
        self.sequence+=1;self.lines.append({'id':self.sequence,'time':datetime.datetime.now().strftime('%H:%M:%S'),'text':line})
        if line=='LINK SAVED restarting':self.ready=False;self.reconnect_after_settings()
        if line.startswith('RECORD phase='):self.health['phase']=line.split('=',1)[1]
        if line.startswith(('LABEL ','LINK ')):
            try:
                data=json.loads(line.split(' ',1)[1])
                if line.startswith('LABEL ') and data.get('id') in LABELS:self.names[data['id']]=data['name']
                elif line.startswith('LINK '):self.link=data
            except (ValueError,KeyError):pass
        fields=dict(re.findall(r'(\w+)=([^\s]+)',line))
        if line.startswith('MOZHANG '):self.version=line;self.ready=False;self.templates={k:0 for k in LABELS};self.motion.clear()
        if line.startswith('STATUS '):
            self.ready=fields.get('calibrated')=='1' and fields.get('IMU')=='OK';self.health.update(fields)
            self.capturing=fields.get('capturing')=='1'
            i=int(fields.get('train','-1'));self.mode=LABELS[i] if 0<=i<len(LABELS) else 'recognize'
        if line.startswith('HEALTH '):
            self.health.update(fields);self.ready=fields.get('ready')=='1'
            if 'train' in fields:
                i=int(fields['train']);self.mode=LABELS[i] if 0<=i<len(LABELS) else 'recognize'
            if 'capturing' in fields:self.capturing=fields['capturing']=='1'
        if line.startswith('READY '):self.ready=True;self.health['bias']=fields.get('bias_dps','')
        if line.startswith('CALIBRATING'):self.ready=False
        m=re.match(r'TEMPLATES (\w+) (\d)/3',line) or re.match(r'TRAIN SAVED (\w+) (\d)/3',line)
        if m and m[1] in LABELS:self.templates[m[1]]=int(m[2])
        if line.startswith('TRAIN ') and line.split()[1] in LABELS:self.mode=line.split()[1]
        if line.startswith('MODE recognize'):self.mode='recognize'
        if line.startswith('CLEARED ') and line[8:] in LABELS:self.templates[line[8:]]=0
        if line.startswith('CAPTURE START'):
            self.capturing=True;self.capture_expected=self.expected;self.capture_mode=fields.get('mode','recognize');self.last_dump=[]
        if line.startswith('CAPTURE END') or line.startswith('CAPTURE CANCEL'):self.capturing=False
        if line.startswith('REJECT ') or line.startswith('RESULT '):
            self.capturing=False
            if self.capture_mode=='recognize':self.history.appendleft({'id':self.sequence,'time':datetime.datetime.now().strftime('%H:%M:%S'),'timestamp':time.time(),'label':line.split()[1] if line.startswith('RESULT ') else 'unknown','expected':self.capture_expected,'distance':fields.get('distance'),'margin':fields.get('margin'),'message':line})
        if line.startswith('DATA BEGIN'):self.dump=[]
        elif line.startswith('DATA END'):self.last_dump=self.dump[:]
        elif line.startswith('DATA '):
            try:self.dump.append([int(x) for x in line[5:].split(',')])
            except ValueError:pass
    def snapshot(self):
        with self.lock:
            connected=bool(self.serial and self.serial.is_open)
            return {'names':dict(self.names),'link':dict(self.link),'flash':dict(self.flash),'connected':connected,'port':self.port,'error':self.error,'ready':self.ready and connected and time.time()-self.last_seen<4,'stale':connected and time.time()-self.last_seen>4,'capturing':self.capturing,'mode':self.mode,'version':self.version,'health':dict(self.health),'templates':dict(self.templates),'motion':list(self.motion),'history':list(self.history),'lines':list(self.lines),'expected':self.expected,'features':self.last_dump[:]}

device=Device()
class Handler(BaseHTTPRequestHandler):
    def log_message(self,*args):pass
    def respond(self,value,status=200,ctype='application/json'):
        data=json.dumps(value,ensure_ascii=False).encode() if ctype=='application/json' else value
        self.send_response(status);self.send_header('Content-Type',ctype+'; charset=utf-8');self.send_header('Content-Length',str(len(data)));self.send_header('Cache-Control','no-store');self.end_headers();self.wfile.write(data)
    def do_GET(self):
        path=urlparse(self.path).path
        if path=='/api/info':return self.respond({'app':'mozhang-studio','version':'0.7','instance':hashlib.sha256(str(ROOT.parent).encode()).hexdigest()[:16]})
        if path=='/api/state':return self.respond(device.snapshot())
        if path=='/api/ports':return self.respond([{'port':p.device,'name':p.description,'recommended':(p.vid,p.pid)==(0x303A,0x1001)} for p in list_ports.comports() if (p.vid,p.pid)==(0x303A,0x1001)])
        files={'/presentation.js':'presentation.js','/':'index.html','/app.js':'app.js','/style.css':'style.css'}
        if path in files:
            file=ROOT/'static'/files[path];data=file.read_bytes().replace(b'__TOKEN__',TOKEN.encode())
            return self.respond(data,ctype={'/presentation.js':'text/javascript','/':'text/html','/app.js':'text/javascript','/style.css':'text/css'}[path])
        return self.respond({'error':'not found'},404)
    def do_POST(self):
        if self.headers.get('X-Wand-Token')!=TOKEN:return self.respond({'error':'invalid token'},403)
        try:
            n=int(self.headers.get('Content-Length',0))
            if n>2048:raise ValueError('请求过大')
            data=json.loads(self.rfile.read(n));path=urlparse(self.path).path
            if path=='/api/shutdown':
                self.respond({'ok':True})
                threading.Thread(target=self.server.shutdown,daemon=True).start()
                return
            if path=='/api/connect':device.connect(data['port'])
            elif path=='/api/disconnect':device.disconnect()
            elif path=='/api/flash':device.flash_firmware(data['port'])
            elif path=='/api/command':device.send(data['command'])
            elif path=='/api/rename':
                ident=data.get('id');name=data.get('name','').strip()
                if ident not in LABELS[6:] or not name or len(name.encode('utf8'))>48 or any(ord(c)<32 for c in name):raise ValueError('自定义名称需为 1–48 字节，不能含换行')
                device.enqueue('rename '+json.dumps({'id':ident,'name':name},ensure_ascii=False,separators=(',',':')))
            elif path=='/api/link':
                mode=data.get('mode');allowed_keys={'mode','ssid','wifi_password','host','port','username','mqtt_password','topic'}
                if mode not in ['usb','mqtt','ble'] or set(data)-allowed_keys:raise ValueError('无效通信配置')
                if mode=='mqtt':
                    if not isinstance(data.get('port'),int) or not 1<=data['port']<=65535:raise ValueError('端口需在 1–65535 之间')
                    for key,limit,required in [('ssid',32,True),('wifi_password',64,False),('host',128,True),('username',64,False),('mqtt_password',128,False),('topic',96,True)]:
                        value=data.get(key,'')
                        if not isinstance(value,str) or len(value.encode('utf8'))>limit or (required and not value) or any(ord(c)<32 for c in value):raise ValueError('通信字段无效：'+key)
                    if any(c in data['host'] for c in '/@: ') or any(c in data['topic'] for c in '+#'):raise ValueError('服务器填主机名/IP；主题不能含通配符')
                raw=json.dumps(data,ensure_ascii=False,separators=(',',':'))
                if len(raw.encode('utf8'))>=750:raise ValueError('配置过长')
                device.enqueue('link '+raw)
            elif path=='/api/expected':
                if data.get('label') not in LABELS+[None]:raise ValueError('无效手势')
                with device.lock:device.expected=data.get('label')
            elif path=='/api/export':
                folder=ROOT.parent/'exports';folder.mkdir(exist_ok=True)
                path=folder/(datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')+'-test.json')
                path.write_text(json.dumps(device.snapshot(),ensure_ascii=False,indent=2),encoding='utf8')
                return self.respond({'ok':True,'path':str(path)})
            else:raise ValueError('未知请求')
            return self.respond({'ok':True})
        except (ValueError,KeyError,OSError,serial.SerialException) as e:return self.respond({'error':str(e)},400)

if __name__=='__main__':
    port=int(os.environ.get('MOZHANG_PORT','8765'))
    print(f'MOZHANG Studio http://127.0.0.1:{port}',flush=True)
    server=ThreadingHTTPServer(('127.0.0.1',port),Handler)
    try:server.serve_forever()
    finally:device.disconnect();server.server_close()
