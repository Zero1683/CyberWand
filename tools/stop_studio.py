import hashlib,json,re,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];instance=hashlib.sha256(str(ROOT).encode()).hexdigest()[:16]
for port in range(8765,8786):
 base=f'http://127.0.0.1:{port}'
 try:
  if json.load(urllib.request.urlopen(base+'/api/info',timeout=.3)).get('instance')!=instance:continue
  page=urllib.request.urlopen(base,timeout=1).read().decode();token=re.search("WAND_TOKEN='([^']+)'",page)[1]
  req=urllib.request.Request(base+'/api/shutdown',data=b'{}',headers={'Content-Type':'application/json','X-Wand-Token':token})
  urllib.request.urlopen(req,timeout=3).close();print('Studio stopped');break
 except (OSError,ValueError):continue
else:print('No matching Studio running')
