#!/usr/bin/env python3
"""Run the supplied Lab 3 programs and retain each correctness/timing log."""
import argparse
import csv
import os
from pathlib import Path
import re
import subprocess


def small_oracles():
    # Independent Bellman-Ford reference, rather than another Floyd loop.
    values = list(map(int, Path('data/graph5.txt').read_text().split()))
    n, weights = values[0], values[1:]
    inf = 1000000000
    expected = []
    for source in range(n):
        distances = [inf] * n
        distances[source] = 0
        for _ in range(n - 1):
            previous = distances[:]
            for u in range(n):
                for v in range(n):
                    w = weights[u*n+v]
                    if previous[u] != inf and w != inf:
                        distances[v] = min(distances[v], previous[u] + w)
        expected.append(distances)
    values = list(map(int, Path('data/graph5_tri.txt').read_text().split()))
    n, adj = values[0], values[1:]
    # Count closed walks of length three, then divide by six.
    closed_walks = sum(adj[i*n+j] * adj[j*n+k] * adj[k*n+i]
                       for i in range(n) for j in range(n) for k in range(n))
    assert closed_walks % 6 == 0
    return expected, closed_walks // 6


def number(text, label):
    match = re.search(r'^' + re.escape(label) + r'\s*:\s*([0-9.]+)', text, re.M)
    if not match:
        raise ValueError('Missing output field: ' + label)
    return match.group(1)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', required=True)
    parser.add_argument('--threads', nargs='+', type=int, default=[16, 32, 64])
    parser.add_argument('--small', action='store_true')
    args = parser.parse_args()
    output = Path(args.output)
    output.mkdir(exist_ok=False)
    matrix, triangles = small_oracles()
    cases = [('floyd', 'graph5.txt'), ('triangle', 'graph5_tri.txt')]
    if not args.small:
        cases += [('floyd', 'graph500.txt'), ('floyd', 'graph5000.txt'),
                  ('triangle', 'graph500_tri.txt'), ('triangle', 'graph5000_tri.txt'),
                  ('triangle', 'graph10000_tri.txt')]
    failures = 0
    fields = ['algorithm', 'input', 'threads', 'serial_seconds', 'parallel_seconds',
              'speedup', 'efficiency_percent', 'triangles', 'correctness', 'error']
    with (output/'summary.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for algorithm, graph in cases:
            for threads in args.threads:
                env = dict(os.environ, OMP_DYNAMIC='FALSE', OMP_NUM_THREADS=str(threads))
                name = '{}-{}-t{}'.format(algorithm, Path(graph).stem, threads)
                print('START ' + name, flush=True)
                log = output/(name + '.log')
                with log.open('w') as handle:
                    run = subprocess.run(['./build/'+algorithm, 'data/'+graph], env=env,
                                         stdout=handle, stderr=subprocess.STDOUT)
                text = log.read_text()
                row = dict(algorithm=algorithm, input=graph, threads=threads,
                           correctness='FAIL', error='')
                try:
                    if run.returncode != 0:
                        raise ValueError('Exit status {}'.format(run.returncode))
                    if not re.search(r'^Correctness\s*:\s*PASS\s*$', text, re.M):
                        raise ValueError('Serial/parallel comparison did not PASS')
                    if algorithm == 'floyd':
                        if int(number(text, 'Actual threads')) != threads:
                            raise ValueError('Actual thread count differs from request')
                        if graph == 'graph5.txt':
                            lines = text.split('Shortest-path matrix:\n', 1)[1].strip().splitlines()
                            got = [[1000000000 if x == 'INF' else int(x) for x in line.split()]
                                   for line in lines]
                            if got != matrix:
                                raise ValueError('Small graph differs from Bellman-Ford reference')
                    else:
                        row['triangles'] = int(number(text, 'Triangles'))
                        if graph == 'graph5_tri.txt' and row['triangles'] != triangles:
                            raise ValueError('Small graph differs from closed-walk reference')
                    row.update(serial_seconds=number(text, 'Serial time'),
                               parallel_seconds=number(text, 'Parallel time'),
                               speedup=number(text, 'Speedup'),
                               efficiency_percent=number(text, 'Efficiency'), correctness='PASS')
                except (ValueError, IndexError) as exc:
                    row['error'] = str(exc)
                    failures += 1
                writer.writerow(row)
                stream.flush()
                print('{} {} {}'.format(row['correctness'], name, row['error']), flush=True)
    print('COMPLETE: {} runs, {} failures'.format(len(cases)*len(args.threads), failures), flush=True)
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
