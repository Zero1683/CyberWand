import os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];env=os.environ.copy();cache=ROOT/'.cache';(cache/'tmp').mkdir(parents=True,exist_ok=True)
env.update(PLATFORMIO_CORE_DIR=str(cache/'platformio'),TEMP=str(cache/'tmp'),TMP=str(cache/'tmp'),TMPDIR=str(cache/'tmp'),PYTHONUTF8='1')
raise SystemExit(subprocess.call([sys.executable,'-m','platformio','run','--project-dir',str(ROOT/'firmware'),*sys.argv[1:]],env=env))
