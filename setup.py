"""Bootstrap a local environment; no global Python packages are changed."""
import argparse,os,subprocess,sys,venv
from pathlib import Path
ROOT=Path(__file__).resolve().parent
def prepare(dev=False,ble=False):
 if sys.version_info<(3,10):raise SystemExit('Python 3.10+ required')
 tmp=ROOT/'.cache/tmp';tmp.mkdir(parents=True,exist_ok=True)
 os.environ.update(TEMP=str(tmp),TMP=str(tmp),TMPDIR=str(tmp))
 folder=ROOT/'.venv';py=folder/('Scripts/python.exe' if os.name=='nt' else 'bin/python')
 if not py.exists():venv.EnvBuilder(with_pip=True).create(folder)
 env=os.environ.copy();cache=ROOT/'.cache';(cache/'tmp').mkdir(parents=True,exist_ok=True)
 env.update(PIP_CACHE_DIR=str(cache/'pip'),TEMP=str(cache/'tmp'),TMP=str(cache/'tmp'),TMPDIR=str(cache/'tmp'),PYTHONUTF8='1')
 req='requirements-dev.txt' if dev else 'requirements.txt'
 marker=folder/('.installed-'+req)
 content=(ROOT/req).read_text()+(ROOT/'requirements.txt').read_text()
 if not marker.exists() or marker.read_text()!=content:
  subprocess.check_call([str(py),'-m','pip','install','-r',str(ROOT/req)],env=env);marker.write_text(content)
 if ble:subprocess.check_call([str(py),'-m','pip','install','-r',str(ROOT/'requirements-ble.txt')],env=env)
 return py
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--dev',action='store_true');p.add_argument('--ble',action='store_true');a=p.parse_args();print(prepare(a.dev,a.ble))
