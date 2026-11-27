#!/usr/bin/env python3
"""Motion-to-PC latency of one Index controller (the probe), old timing against new, on one dongle with the driver
running and LH_CLOCK_TRACE recording every IMU sample (PC time, device time). The probe's own cable gives the
reference: plugged in at the start and at the end (its clock keeps running in between), so the PC time of each
sample's moment is known and the delay is absolute, not "over the fastest sample".

  latencyrun.py PROBE_SLOT ORDER OUT
      ORDER: comma-separated slots, the probe's companions in the order they're added (1..N devices)
Steps (both timings each): devices 1..N with no loss, then 8 devices with 0/5/10/15/20 % of answers dropped.
"old": the join window in every cycle and the Index radios answering at once (as v1.0.4);
"new": the join window only when a beacon goes out, Index radios answering just before their poll.
Marks go to OUT/marks.jsonl (PC monotonic time, what was set)."""
import json, os, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hid
import dongle
from air_update import Air
from doorman_link import Dongle

SETTLE, SECS = 5, 15


def wired():
    return any(d['product_id'] == 0x2300 for d in hid.enumerate(0x28de, 0))


def main():
    probe = int(sys.argv[1]); order = [int(x) for x in sys.argv[2].split(',')]; out = sys.argv[3]
    os.makedirs(out, exist_ok=True)
    marks = open(os.path.join(out, 'marks.jsonl'), 'a')
    mark = lambda **k: (marks.write(json.dumps({'t': time.monotonic(), **k}) + '\n'), marks.flush())
    index_slots = [s for s in [probe] + order if s in (3, 4, 5)]
    h = dongle.open_dev()
    sw = lambda what, a=0, b=0: dongle.cmd(h, 0x3B, bytes([what, a, b]))

    def timing(old):
        sw(0, 1 if old else 0)
        with Dongle() as d:
            for s in index_slots:
                for _ in range(3):
                    try: Air(d, s).req('A', bytes([0 if old else 1])); break
                    except TimeoutError: pass

    def devices(n):
        keep = {probe, *order[:n - 1]}
        mask = sum(1 << s for s in range(10) if s not in keep)
        sw(1, mask & 0xFF, mask >> 8)

    def window(**k):
        time.sleep(SETTLE)
        mark(event='start', **k); time.sleep(SECS); mark(event='end', **k)
        print(k, flush=True)

    print('plug the probe in by cable', flush=True)
    while not wired(): time.sleep(0.5)
    time.sleep(5); mark(event='start', phase='wired1'); time.sleep(30); mark(event='end', phase='wired1')
    print('unplug it now', flush=True)
    while wired(): time.sleep(0.5)
    time.sleep(10)
    try:
        for old in (True, False):
            timing(old)
            for n in range(1, len(order) + 2):
                devices(n); window(phase='devices', timing='old' if old else 'new', devices=n)
            devices(8)
            for loss in (0, 5, 10, 15, 20):
                sw(2, loss); window(phase='loss', timing='old' if old else 'new', devices=8, loss=loss)
            sw(2, 0)
    finally:
        sw(0, 0); sw(1, 0, 0); sw(2, 0); timing(False)
    print('plug the probe in by cable again', flush=True)
    while not wired(): time.sleep(0.5)
    time.sleep(5); mark(event='start', phase='wired2'); time.sleep(30); mark(event='end', phase='wired2')
    print('done', flush=True)


if __name__ == '__main__':
    main()
