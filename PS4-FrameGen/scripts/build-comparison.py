#!/usr/bin/env python3
"""Embed measured rasters in a standalone comparator. No images are retouched."""
import base64,io,json,pathlib,sys
import numpy as np
from PIL import Image

def main():
 if len(sys.argv)!=3:raise SystemExit('build-comparison.py QUALITY_DIRECTORY NEW_OUTPUT.html')
 directory=pathlib.Path(sys.argv[1]);output=pathlib.Path(sys.argv[2])
 results=json.loads((directory/'summary.json').read_text())['cases'];images={}
 def embed(name,image):
  stream=io.BytesIO();image.save(stream,format='PNG');images[name]='data:image/png;base64,'+base64.b64encode(stream.getvalue()).decode()
 for item in results:embed(item['name'],Image.open(directory/item['name']/'generated.png'))
 embed('original-A-scaled-720',Image.open(directory/'reference/frame_0.ppm'))
 embed('original-B-scaled-720',Image.open(directory/'reference/frame_2.ppm'))
 truth=Image.open(directory/'v03-720/reference.png');embed('withheld-reference',truth)
 candidate=np.asarray(Image.open(directory/'v03-360/generated.png'),dtype=np.int16)
 target=np.asarray(truth,dtype=np.int16);embed('difference-x4-360',Image.fromarray(np.minimum(255,np.abs(candidate-target)*4).astype(np.uint8)))
 html='''<!doctype html><meta charset="utf-8"><title>PS4 FrameGen 0.3: comparador medido</title>
<style>body{font:16px system-ui;background:#111827;color:#eee;max-width:1100px;margin:2em auto}select,input{margin:1em}#view{position:relative;aspect-ratio:16/9}img{position:absolute;width:100%;height:100%}#right{clip-path:inset(0 0 0 50%)}</style>
<h1>PS4 FrameGen 0.3 — comparación CPU host</h1><p>Imágenes reales, referencias de un vídeo de 25 Hz y diferencia amplificada ×4. PSNR no garantiza calidad perceptual. Sin ejecución PS4/GPU.</p>
<label>Izquierda <select id="leftChoice"></select></label><label>Derecha <select id="rightChoice"></select></label>
<label>Separador <input id="split" type="range" min="0" max="100" value="50"></label><div id="view"><img id="left" alt="Comparación izquierda"><img id="right" alt="Comparación derecha"></div>
<pre id="metrics"></pre><script>const images=IMAGES,results=RESULTS;
const leftChoice=document.getElementById('leftChoice'),rightChoice=document.getElementById('rightChoice'),split=document.getElementById('split'),left=document.getElementById('left'),right=document.getElementById('right'),metrics=document.getElementById('metrics');
for(const control of [leftChoice,rightChoice])for(const name of Object.keys(images)){let o=document.createElement('option');o.value=name;o.textContent=name;control.append(o)}
leftChoice.value='v03-720';rightChoice.value='v03-360';function update(){left.src=images[leftChoice.value];right.src=images[rightChoice.value];right.style.clipPath=`inset(0 0 0 ${split.value}%)`;metrics.textContent=[leftChoice.value,rightChoice.value].map(name=>{let r=results.find(x=>x.name===name);return r?name+' '+JSON.stringify(r.fixed_reference_quality):name}).join('\\n')}
leftChoice.onchange=rightChoice.onchange=split.oninput=update;update();</script>'''
 html=html.replace('IMAGES',json.dumps(images)).replace('RESULTS',json.dumps(results))
 with output.open('x') as f:f.write(html)
 print('Embedded',len(images),'measured rasters;',output)
if __name__=='__main__':main()
