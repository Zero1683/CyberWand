import hashlib,json,os,socket,subprocess,sys,time,urllib.request,webbrowser
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from setup import prepare
def info(port):
 try:
  with urllib.request.urlopen(f'http://127.0.0.1:{port}/api/info',timeout=.3) as r:return json.load(r)
 except Exception:return {}
def main():
 py=prepare();instance=hashlib.sha256(str(ROOT).encode()).hexdigest()[:16]
 for port in range(8765,8786):
  if info(port).get('instance')==instance:break
  with socket.socket() as s:
   try:s.bind(('127.0.0.1',port))
   except OSError:continue
  (ROOT/'logs').mkdir(exist_ok=True)
  env=os.environ.copy();env['MOZHANG_PORT']=str(port);env['PYTHONUTF8']='1'
  with (ROOT/'logs/studio.log').open('ab') as log:
   child=subprocess.Popen([str(py),str(ROOT/'studio/server.py')],cwd=ROOT,env=env,stdout=log,stderr=log,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
  for _ in range(100):
   if info(port).get('instance')==instance:break
   if child.poll() is not None:raise SystemExit('Studio failed; see logs/studio.log')
   time.sleep(.1)
  else:raise SystemExit('Studio startup timeout; see logs/studio.log')
  break
 else:raise SystemExit('No free localhost port in 8765–8785')
 url=f'http://127.0.0.1:{port}/';print(url)
 if '--no-browser' not in sys.argv:webbrowser.open(url)
if __name__=='__main__':main()
