"""Software-only remote control tests; no serial, MQTT or motor access."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
FLAGS=['-std=c11','-Wall','-Wextra','-Werror','-Wno-type-limits','-Wno-sign-compare',
       '-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-Iinclude',
       '-DMOTION_LINE_DEMO=1','-DMOTION_LINE_CIRCUIT=1','-DMOTION_LINE_ADAPTIVE=1',
       '-DMOTION_LINE_ROUTE=1','-DMOTION_LINE_REMOTE=1','-DBARCODE_SLOW_SCAN=1','-DBARCODE_WIDE=1']
with tempfile.TemporaryDirectory(prefix='robocar-remote-') as folder:
 for name in ['wire','console','watchdog','settings']:
  exe=str(Path(folder)/(name+'.exe'))
  sources=['testing/subsystem2/'+('route_settings' if name=='settings' else 'remote_'+name)+'_test.c']
  if name!='wire':sources+=['app_program/subsystem2/motion_logic.c']
  subprocess.run(['gcc',*FLAGS,*sources,'-o',exe],cwd=ROOT,check=True)
  subprocess.run([exe],check=True)
 with_sources=['testing/subsystem2/unlimited_run_test.c','app_program/subsystem2/motion_logic.c']
 exe=str(Path(folder)/'unlimited.exe')
 subprocess.run(['gcc',*FLAGS,'-DMOTION_LINE_UNLIMITED=1',*with_sources,'-o',exe],cwd=ROOT,check=True)
 subprocess.run([exe],check=True)
print('PASS remote command transport, actual GP20 console arbiter and actual motor watchdog')
