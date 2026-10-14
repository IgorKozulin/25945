"""Behavior checks using only the Python standard library; run with make test."""
from pathlib import Path
import os
import subprocess
import tempfile
import time
import unittest
from datetime import datetime
from zoneinfo import ZoneInfo

ROOT = Path(__file__).resolve().parent


def run(task, program, *args, data='', env=None):
    return subprocess.run([str(ROOT / task / program), *map(str, args)],
                          input=data, text=True, capture_output=True, timeout=12, env=env)


class Tasks(unittest.TestCase):
    def test_options(self):
        self.assertEqual(run('t1', 'options').returncode, 0)
        for args in [('-x',), ('-U',), ('-U-5',), ('-Ugarbage',), ('-U99999999999999999999999999',), ('-V=no',), ('extra',)]:
            self.assertNotEqual(run('t1', 'options', *args).returncode, 0, args)
        out = run('t1', 'options', '-u', '-U32', '-u', '-U64')
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertEqual(out.stdout.splitlines(), ['64', '32'])
        out = run('t1', 'options', '-v', '-VBSA_CHECK=a=b')
        self.assertIn('BSA_CHECK=a=b\n', out.stdout)
        out = run('t1', 'options', '-pi')
        self.assertTrue(out.stdout.startswith('UID='))
        self.assertEqual(out.returncode, 0)
        out = run('t1', 'options', '-p', '-s')
        parts = dict(item.split('=') for item in out.stdout.split())
        self.assertEqual(parts['PID'], parts['PGID'])
        self.assertEqual(run('t1', 'options', '-c', '-C0').stdout.strip(), '0')
        self.assertEqual(Path(run('t1', 'options', '-d').stdout.strip()), Path.cwd())

    def test_time(self):
        before = datetime.now(ZoneInfo('America/Los_Angeles')).replace(tzinfo=None)
        result = run('t2', 'ex_time')
        after = datetime.now(ZoneInfo('America/Los_Angeles')).replace(tzinfo=None)
        self.assertEqual(result.returncode, 0)
        date, clock, zone = result.stdout.split()
        actual = datetime.strptime(date + ' ' + clock, '%m/%d/%Y %H:%M:%S')
        self.assertLessEqual(abs((actual - before).total_seconds()), 2)
        self.assertLessEqual(abs((actual - after).total_seconds()), 2)
        self.assertIn(zone, ('PST', 'PDT'))

    def test_uid(self):
        with tempfile.TemporaryDirectory() as directory:
            file = Path(directory) / 'private.txt'
            file.write_text('owner-only\n'); file.chmod(0o600)
            result = run('t3', 'setuid_test', file)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stdout.count('File opened successfully'), 2)
            self.assertEqual(run('t3', 'setuid_test', file.with_name('missing')).stderr.count('No such file'), 2)

    def test_list(self):
        text = 'first\n\n' + 'x' * 20000 + '\n'
        self.assertEqual(run('t4', 'list_strings', data=text + '.stop\nignored\n').stdout, text)
        self.assertEqual(run('t4', 'list_strings', data='last').stdout, 'last')
        self.assertEqual(run('t4', 'list_strings', data='.\n').stdout, '')

    def test_lookups(self):
        with tempfile.TemporaryDirectory() as directory:
            file = Path(directory) / 'lines.txt'
            for task, program in [('t5', 'line_lookup'), ('t6', 'line_lookup_alarm'), ('t7', 'line_lookup_mmap')]:
                with self.subTest(task=task):
                    file.write_text('alpha\n\n' + 'z' * 12000 + '\nlast')
                    result = run(task, program, file, data='bad\n-1\n999\n2\n4\n3\n0\n')
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn('Line 2: Offset = 6, Length = 1', result.stdout)
                    self.assertIn('Line 4: Offset = 12008, Length = 4', result.stdout)
                    self.assertIn('\nlast\n', result.stdout)
                    self.assertIn('z' * 12000, result.stdout)
                    self.assertEqual(result.stderr.count('Invalid line number'), 3)
                    self.assertEqual(run(task, program, file, data='').returncode, 0)
                    file.write_text('')
                    self.assertIn('File is empty', run(task, program, file).stdout)
                    file.write_text('one\n')
                    self.assertNotIn('Line 2:', run(task, program, file, data='0\n').stdout)
                    self.assertNotEqual(run(task, program, file.with_name('missing')).returncode, 0)

    def test_timeouts(self):
        with tempfile.TemporaryDirectory() as directory:
            file = Path(directory) / 'lines.txt'; file.write_text('alpha\nbeta\nlast')
            for task, program in [('t6', 'line_lookup_alarm'), ('t7', 'line_lookup_mmap')]:
                with self.subTest(task=task):
                    proc = subprocess.Popen([str(ROOT / task / program), str(file)], stdin=subprocess.PIPE,
                                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                    # A partial line must not disable the timer.
                    proc.stdin.write('1'); proc.stdin.flush()
                    start = time.monotonic()
                    proc.wait(timeout=8)
                    elapsed = time.monotonic() - start
                    output = proc.stdout.read(); error = proc.stderr.read()
                    proc.stdin.close(); proc.stdout.close(); proc.stderr.close()
                    self.assertEqual(proc.returncode, 0, error)
                    self.assertGreater(elapsed, 4)
                    self.assertLess(elapsed, 7)
                    self.assertTrue(output.endswith('[TIMEOUT] Full file:\nalpha\nbeta\nlast'))

    def test_editor_lock(self):
        with tempfile.TemporaryDirectory() as directory:
            file = Path(directory) / "a ' quoted; name.txt"; file.write_text('original\n')
            marker = Path(directory) / 'started'
            helper = Path(directory) / 'editor.py'
            helper.write_text('import pathlib,sys,time\npathlib.Path(sys.argv[1]).write_text("started")\ntime.sleep(2)\n')
            env = os.environ.copy()
            import shlex, sys
            env['EDITOR'] = f'{shlex.quote(sys.executable)} {shlex.quote(str(helper))} {shlex.quote(str(marker))}'
            proc = subprocess.Popen([str(ROOT / 't8/locked_editor'), str(file)], env=env,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                deadline = time.monotonic() + 4
                while not marker.exists() and time.monotonic() < deadline:
                    time.sleep(0.02)
                self.assertTrue(marker.exists())
                result = run('t8', 'locked_editor', file, env={**os.environ, 'EDITOR': 'true'})
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('locked', result.stderr)
            finally:
                out, err = proc.communicate(timeout=6)
            self.assertEqual(proc.returncode, 0, err)
            self.assertEqual(run('t8', 'locked_editor', file, env={**os.environ, 'EDITOR': 'true'}).returncode, 0)
            self.assertEqual(file.read_text(), 'original\n')
            if os.uname().sysname != 'SunOS':
                self.assertNotEqual(run('t8', 'locked_editor', '--mandatory', file).returncode, 0)

    def test_child(self):
        with tempfile.TemporaryDirectory() as directory:
            file = Path(directory) / '-long.txt'
            content = ''.join(f'Line {n}\n' for n in range(10000)); file.write_text(content)
            result = run('t9', 'process_manager', file)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn(content, result.stdout)
            self.assertTrue(result.stdout.endswith('[PARENT] Child has finished. This is the last line.\n'))
            self.assertNotEqual(run('t9', 'process_manager', file.with_name('missing')).returncode, 0)


if __name__ == '__main__':
    unittest.main(verbosity=2)
