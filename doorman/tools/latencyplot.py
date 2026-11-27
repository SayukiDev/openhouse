#!/usr/bin/env python3
"""Graphs for latencyrun.py: motion-to-PC delay of the probe controller, v1.0.4 timing against new.

  latencyplot.py OUT PROBE_SERIAL [PNG]

Delay of a sample = when it reached the PC - when the controller sampled it. The sampling moment on the PC's clock
comes from the probe's cable runs before and after (the fastest cable deliveries, with the drift between the two
fitted out), plus the cable's own fastest path, which is taken as 0: the numbers are "over the cable's fastest". The
Doorman path's trace times are put back to real arrival (the driver subtracts an assumed 1.5 ms before tracing)."""
import json, sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

DOORMAN_SHIFT = 0.0015


def main():
    out, probe = sys.argv[1], sys.argv[2]
    png = sys.argv[3] if len(sys.argv) > 3 else f'{out}/latency.png'
    marks = [json.loads(l) for l in open(f'{out}/marks.jsonl')]
    rows = [l.split() for l in open(f'{out}/trace.txt')]
    h = np.array([float(r[1]) for r in rows if r[0] == probe])
    d = np.array([float(r[2]) for r in rows if r[0] == probe])

    def spans(**k):
        st = [m['t'] for m in marks if m['event'] == 'start' and all(m.get(a) == b for a, b in k.items())]
        en = [m['t'] for m in marks if m['event'] == 'end' and all(m.get(a) == b for a, b in k.items())]
        return list(zip(st, en))

    def sel(span):
        return (h >= span[0]) & (h <= span[1])

    # the cable's fastest deliveries per second, both cable runs: offset = a + b * t
    xs, ys = [], []
    for ph in ('wired1', 'wired2'):
        for sp in spans(phase=ph):
            m = sel(sp)
            hh, oo = h[m], (h - d)[m]
            for s in np.unique(np.floor(hh)):
                k = np.floor(hh) == s
                xs.append(s + 0.5); ys.append(oo[k].min())
    xs, ys = np.array(xs), np.array(ys)
    b, a = np.polyfit(xs - xs.mean(), ys, 1)
    ref = lambda t: a + b * (t - xs.mean())
    print(f'cable fit: drift {b * 1e6:+.1f} ppm, residual {np.std(ys - ref(xs)) * 1000:.2f} ms over {len(xs)} s')
    wired_all = np.concatenate([((h - d) - ref(h))[sel(sp)] for ph in ('wired1', 'wired2') for sp in spans(phase=ph)]) * 1000
    cable = float(np.median(wired_all))

    def delay(span):
        m = sel(span)
        return ((h[m] + DOORMAN_SHIFT - d[m]) - ref(h[m])) * 1000

    res = {}
    for m in marks:
        if m['event'] != 'start' or m.get('phase') not in ('devices', 'loss'): continue
        k = (m['phase'], m['timing'], m['devices'], m.get('loss', 0))
        end = next(x['t'] for x in marks if x['event'] == 'end' and x['t'] > m['t'])
        v = delay((m['t'], end))
        res[k] = (float(np.median(v)), float(np.percentile(v, 90)), len(v))
    for k in sorted(res): print(k, 'median %.2f p90 %.2f n %d' % res[k])
    json.dump({'cable_median_ms': cable, 'drift_ppm': b * 1e6, 'steps': {'|'.join(map(str, k)): v for k, v in res.items()}},
              open(f'{out}/results.json', 'w'), indent=1)

    red, blue = '#d62728', '#1f77b4'
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14.5, 5.6))
    fig.suptitle('Index controller: delay from motion to the PC (lower is better)', fontsize=13)
    devs = sorted({k[2] for k in res if k[0] == 'devices'})
    for timing, col, lab in (('old', red, 'v1.0.4'), ('new', blue, 'new')):
        xs_ = [n for n in devs if timing == 'new' or n <= 8]   # v1.0.4 took up to 8 devices
        ys_ = [res[('devices', timing, n, 0)][0] for n in xs_]
        ax1.plot(xs_, ys_, 'o-', color=col, lw=2, label=lab)
    y8 = res[('devices', 'old', 8, 0)][0]; y10 = res[('devices', 'new', 10, 0)][0]
    ax1.annotate('v1.0.4 max is 8', xy=(8, y8), xytext=(5.4, y8 + 3), color='0.35', fontsize=11,
                 arrowprops=dict(arrowstyle='->', color='0.35'))
    ax1.annotate('new goes up to 10', xy=(10, y10), xytext=(8.4, y8 + 4.2), color=blue, fontsize=11,
                 arrowprops=dict(arrowstyle='->', color=blue))
    ax1.axhline(cable, color='0.45', ls='--', lw=1.3, label='same controller on a cable')
    ax1.set_title('by number of devices (no packet loss added)')
    ax1.set_xlabel('devices on one dongle (3 index controllers + the rest trackers)')
    ax1.set_ylabel('delay (ms)'); ax1.set_xticks(devs); ax1.set_ylim(0, max(y8, y10) + 6); ax1.grid(True); ax1.legend(loc='upper left')
    losses = sorted({k[3] for k in res if k[0] == 'loss'})
    for timing, col, lab in (('old', red, 'v1.0.4'), ('new', blue, 'new')):
        ys_ = [res[('loss', timing, 8, L)][0] for L in losses]
        ax2.plot(losses, ys_, 'o-', color=col, lw=2, label=lab)
    ax2.axhline(cable, color='0.45', ls='--', lw=1.3, label='same controller on a cable')
    ax2.set_title('8 devices on one dongle, with packet loss')
    ax2.set_xlabel('packet loss %'); ax2.set_ylabel('delay (ms)'); ax2.set_xticks(losses); ax2.set_ylim(bottom=0)
    ax2.grid(True); ax2.legend(loc='upper left')
    fig.tight_layout()
    fig.savefig(png, dpi=110)
    print('saved', png)


if __name__ == '__main__':
    main()
