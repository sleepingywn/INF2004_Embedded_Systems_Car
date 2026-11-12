"""Software-only digital capture, decoder and actual owner/telemetry checks."""
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[2]
FLAGS = ['-std=c11','-Wall','-Wextra','-Werror','-Wno-type-limits','-Wno-sign-compare',
 '-Iinclude','-DIR_BARCODE_DIGITAL=1','-DBARCODE_SLOW_SCAN=1','-DBARCODE_WIDE=1',
 '-DMOTION_LINE_DEMO=1','-DMOTION_LINE_CIRCUIT=1','-DMOTION_LINE_ADAPTIVE=1',
 '-DMOTION_LINE_ROUTE=1']
with tempfile.TemporaryDirectory(prefix='barcode-edges-') as temp:
 for name, sources in [
  ('edges',['testing/subsystem2/digital_edges_test.c','app_program/subsystem3/barcode_sense.c']),
  ('handoff',['testing/subsystem2/digital_handoff_test.c','app_program/subsystem2/motion_logic.c','app_program/subsystem3/barcode_sense.c','app_program/subsystem3/sensor_line.c']),
  ('telemetry',['testing/subsystem2/barcode_telemetry_test.c','app_program/subsystem2/motion_logic.c'])]:
  exe = str(Path(temp)/(name+'.exe'))
  subprocess.run(['gcc',*FLAGS,*sources,'-o',exe],cwd=ROOT,check=True)
  subprocess.run([exe],cwd=ROOT,check=True)
