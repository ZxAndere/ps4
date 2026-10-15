#!/usr/bin/env python3
"""Measured CPU image-file workflow; never reports display FPS or PS4 performance."""
import argparse
import hashlib
import json
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]

def run(args):
    subprocess.run([str(v) for v in args], check=True)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', type=pathlib.Path, default=ROOT/'build/framegen_cli')
    parser.add_argument('--frames', type=pathlib.Path, help='directory containing OpenCV 1080p_00..04.png')
    parser.add_argument('--video', type=pathlib.Path, help='CFR video; extract five consecutive originals with verified timestamps')
    parser.add_argument('--output', type=pathlib.Path, default=ROOT/'artifacts/benchmark')
    parser.add_argument('--repeats', type=int, default=3)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    cases = [('native', 'native'), ('native', '1080'), ('native', '720'), ('1080', '1080'), ('720', '720')]
    records = []
    sources = []
    original_rate = None
    if args.video and args.frames:
        parser.error('Use --video or --frames, not both')
    if args.video:
        probe = json.loads(subprocess.check_output(['ffprobe', '-v', 'error', '-select_streams', 'v:0',
            '-read_intervals', '%+#5', '-show_entries', 'frame=best_effort_timestamp_time', '-of', 'json', str(args.video)]))
        timestamps = [float(f['best_effort_timestamp_time']) for f in probe['frames']]
        if len(timestamps) != 5 or timestamps[1] <= timestamps[0]:
            raise ValueError('Need five timestamped frames')
        interval = timestamps[1] - timestamps[0]
        if any(abs((timestamps[i]-timestamps[i-1])-interval) > 1e-5 for i in range(2, 5)):
            raise ValueError('Variable frame spacing: cannot assume uniformly spaced truth images')
        original_rate = 1 / interval
        args.frames = args.output/'consecutive'
        args.frames.mkdir(exist_ok=True)
        run(['ffmpeg', '-loglevel', 'error', '-y', '-i', args.video, '-frames:v', '5', '-start_number', '0',
             args.frames/'1080p_%02d.png'])
        sources.append({'name': args.video.name, 'sha256': hashlib.sha256(args.video.read_bytes()).hexdigest(),
                        'timestamps_seconds': timestamps, 'decoded_source_rate': original_rate})
    if args.frames:
        decoded = args.output/'decoded'
        decoded.mkdir(exist_ok=True)
        for i in range(5):
            source = args.frames/f'1080p_{i:02d}.png'
            sources.append({'name': source.name, 'sha256': hashlib.sha256(source.read_bytes()).hexdigest()})
            run(['ffmpeg', '-loglevel', 'error', '-y', '-i', source, '-frames:v', '1', decoded/f'{i}.ppm'])
    for source_name in (['analytic', 'opencv'] if args.frames else ['analytic']):
        for multiplier in (2, 3, 4):
            for processing, output in cases:
                destination = args.output/f'{source_name}_x{multiplier}_{processing}_{output}'
                command = [args.executable, '--multiplier', multiplier, '--process', processing,
                           '--output', output, '--output-dir', destination, '--repeats', args.repeats]
                if source_name == 'analytic':
                    command += ['--demo']
                else:
                    command += ['--input', decoded/'0.ppm', '--next', decoded/f'{multiplier}.ppm']
                    if original_rate:
                        command += ['--source-fps', original_rate / multiplier]
                    # Frames between the endpoints are withheld from the estimator.
                    for i in range(1, multiplier):
                        command += ['--truth', decoded/f'{i}.ppm']
                run(command)
                record = json.loads((destination/'metrics.json').read_text())
                record.update(case=destination.name, processing_requested=processing, output_requested=output)
                records.append(record)
    # Compare both supported filters on the same native source.
    for resolution in ('1080', '720'):
        for filt in ('area', 'bilinear'):
            destination = args.output/f'filter_{resolution}_{filt}'
            run([args.executable, '--demo', '--multiplier', 2, '--output', resolution, '--filter', filt,
                 '--output-dir', destination, '--repeats', args.repeats])
            record = json.loads((destination/'metrics.json').read_text())
            record.update(case=destination.name)
            records.append(record)
    (args.output/'summary.json').write_text(json.dumps({'platform': 'host CPU, offline files; not PS4',
        'source_files': sources, 'cases': records}, indent=2)+'\n')
    lines = ['# Resultados CPU reproducibles', '',
             'No son FPS de pantalla ni resultados de PS4. Tiempo por par de originales; incluye todos los intermedios del par. '
             'Mediana de las repeticiones, tras calentamiento. Entrada analítica: 2560×1440; OpenCV: dimensiones decodificadas.', '',
             '| Caso | Entrada | Proceso | Buffer de salida | Total mediana ms | Escalado medio ms | Memoria imágenes MiB |',
             '|---|---|---|---|---:|---:|---:|']
    for record in records:
        dims = lambda key: '×'.join(map(str, record[key]))
        lines.append(f"| {record['case']} | {dims('input')} | {dims('processing')} | {dims('output_buffer')} | "
                     f"{record['median_pair_ms']:.2f} | {record['output_scaling_mean_ms']:.2f} | {record['image_buffer_bytes']/2**20:.1f} |")
    lines += ['', 'La calidad por fotograma está en `summary.json`: PSNR frente a la imagen retenida y frente al original '
              'temporalmente más cercano. PSNR no mide todos los defectos perceptivos. Los recortes sintéticos tienen '
              'movimiento conocido; los fotogramas OpenCV no se usan para ajustar el estimador. '
              'Memoria de imágenes es una contabilidad de buffers, no RSS ni VRAM.', '']
    (args.output/'RESULTS.md').write_text('\n'.join(lines))

if __name__ == '__main__':
    main()
