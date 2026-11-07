"""Run legacy motor/IR/GP20 regression suites with host GCC; no hardware access.

Run from any directory: python testing/subsystem2/run_regression_tests.py
Requires Python 3 and gcc on PATH. Build artifacts use an isolated temp folder.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    with tempfile.TemporaryDirectory(prefix="robocar-regression-") as temp:
        checks = []

        def run(name, args):
            binary = str(Path(temp) / (name + ".exe"))
            for command in [["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", *args, "-o", binary], [binary]]:
                result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
                if result.returncode:
                    print(name, result.stdout, result.stderr)
                    raise SystemExit(result.returncode)
            checks.append(name)
            print("PASS", name, flush=True)

        for name,flags in [('default',['-DMOTION_LINE_DEMO=0']),('usb2',['-DMOTION_LINE_DEMO=1']),('button30',['-DMOTION_LINE_DEMO=1','-DMOTION_LINE_CIRCUIT=1']),('auto30',['-DMOTION_LINE_DEMO=1','-DMOTION_LINE_CIRCUIT=1','-DMOTION_LINE_AUTOSTART=1'])]:
            run('motion-'+name,flags+['-Wno-type-limits','-Wno-sign-compare','-Iinclude','testing/subsystem2/motion_line_integration_test.c','app_program/subsystem2/motion_logic.c'])
            fixture='line_autostart_console_test.c' if name=='auto30' else 'line_console_test.c'
            run('console-'+name,flags+['-Iinclude','testing/subsystem2/'+fixture])
            if name!='default':run('line-'+name,flags+['testing/subsystem2/line_follow_test.c','app_program/subsystem2/motion_logic.c','app_program/subsystem3/sensor_line.c'])
        run('button',['testing/subsystem2/line_button_test.c'])
        run('boot-cal',['testing/subsystem2/line_boot_calibration_test.c','app_program/subsystem3/sensor_line.c'])
        run('speed-controller',['testing/subsystem2/feedback_bench/controller_test.c','-lm'])
        run('feedback-bench',['-Itesting/subsystem2/feedback_bench/mock','-Itesting/subsystem2/encoder_bench/mock','testing/subsystem2/feedback_bench/host_test.c','app_program/subsystem2/motion_logic.c'])
        run('bootsel-console',['-DMOTION_LINE_DEMO=1','-DMOTION_LINE_CIRCUIT=1','-Iinclude','testing/subsystem2/line_bootsel_console_test.c'])
        run('line-indicator',['-Iinclude','testing/subsystem2/line_indicator_test.c'])
        run('indicator-console',['-DMOTION_LINE_DEMO=1','-DMOTION_LINE_CIRCUIT=1','-DTM_WIFI_CYW43=1','-Iinclude','testing/subsystem2/line_indicator_console_test.c'])
        run('line-low-speed',['-DMOTION_LINE_DEMO=1','-DMOTION_LINE_CIRCUIT=1','testing/subsystem2/line_speed_control_test.c','app_program/subsystem2/motion_logic.c','-lm'])
        for mode,flags in [('default',['-DMOTION_LINE_DEMO=0']),('button30',['-DMOTION_LINE_DEMO=1','-DMOTION_LINE_CIRCUIT=1'])]:
            run('ir-handoff-'+mode,flags+['-Wno-type-limits','-Wno-sign-compare','-Iinclude','testing/subsystem2/ir_handoff_integration_test.c','app_program/subsystem2/motion_logic.c','app_program/subsystem3/sensor_line.c','app_program/subsystem3/barcode_sense.c'])
        run('curve-response',['-DMOTION_LINE_DEMO=1','-DMOTION_LINE_CIRCUIT=1','testing/subsystem2/line_curve_response_test.c','app_program/subsystem2/motion_logic.c','-lm'])
        run('tight-turn',['-DMOTION_LINE_DEMO=1','-DMOTION_LINE_CIRCUIT=1','testing/subsystem2/line_tight_turn_test.c','app_program/subsystem2/motion_logic.c','-lm'])
        run('encoder-reason',['-DMOTION_LINE_DEMO=1','-DMOTION_LINE_CIRCUIT=1','testing/subsystem2/line_encoder_reason_test.c','app_program/subsystem2/motion_logic.c'])
        search_flags=['-DMOTION_LINE_DEMO=1','-DMOTION_LINE_CIRCUIT=1','-DMOTION_LINE_SEARCH=1']
        run('line-search',search_flags+['testing/subsystem2/line_search_test.c','app_program/subsystem2/motion_logic.c','app_program/subsystem3/sensor_line.c'])
        run('search-owner',search_flags+['-Wno-type-limits','-Wno-sign-compare','-Iinclude','testing/subsystem2/line_search_owner_test.c','app_program/subsystem2/motion_logic.c'])
        run('search-console',search_flags+['-Iinclude','testing/subsystem2/line_console_test.c'])
        run('search-handoff',search_flags+['-Wno-type-limits','-Wno-sign-compare','-Iinclude','testing/subsystem2/ir_handoff_integration_test.c','app_program/subsystem2/motion_logic.c','app_program/subsystem3/sensor_line.c','app_program/subsystem3/barcode_sense.c'])
        run('pause-encoder',search_flags+['testing/subsystem2/line_pause_encoder_test.c','app_program/subsystem2/motion_logic.c'])
        run('pause-encoder-normal',['-DMOTION_LINE_DEMO=1','-DMOTION_LINE_CIRCUIT=1','testing/subsystem2/line_pause_encoder_test.c','app_program/subsystem2/motion_logic.c'])
        run('gentle-tracking',search_flags+['testing/subsystem2/line_gentle_tracking_test.c','app_program/subsystem2/motion_logic.c'])
        run('line-startup',search_flags+['testing/subsystem2/line_startup_test.c','app_program/subsystem2/motion_logic.c'])
        run('curve-recovery',search_flags+['testing/subsystem2/line_curve_recovery_test.c','app_program/subsystem2/motion_logic.c','-lm'])
        run('curve-trigger',search_flags+['testing/subsystem2/line_curve_trigger_test.c','app_program/subsystem2/motion_logic.c'])
        run('line-overlap',search_flags+['testing/subsystem2/line_overlap_test.c','app_program/subsystem2/motion_logic.c'])
        run('line-overlap-normal',['-DMOTION_LINE_DEMO=1','-DMOTION_LINE_CIRCUIT=1','testing/subsystem2/line_overlap_test.c','app_program/subsystem2/motion_logic.c'])
        run('overlap-owner',search_flags+['-Wno-type-limits','-Wno-sign-compare','-Iinclude','testing/subsystem2/line_overlap_owner_test.c','app_program/subsystem2/motion_logic.c'])
        run('ir-acquisition',['-Itesting/console/mock','-Iinclude','-Iconfig','testing/console/ir_acquisition_test.c'])
        print('Completed',len(checks),'test executables')


if __name__ == "__main__":
    main()
