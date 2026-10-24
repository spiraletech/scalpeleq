"""Run surgery_onyx_study <directory>, then this script <directory>.
Requires numpy, scipy, matplotlib. Deterministic synthetic sources, not a listening verdict.
"""
from pathlib import Path
import csv,sys,json
import numpy as np
from scipy.io import wavfile
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

root=Path(sys.argv[1]); names=['SPARKLE','SHINE','SMOOTHENING','VINTAGE TAPE']
rows=[]
for r in csv.DictReader((root/'manifest.csv').open()):
    sr=int(float(r['sr']));hz=int(float(r['hz']));level=float(r['level'])
    x=np.fromfile(root/r['file'],dtype='<f4').astype(float)
    spec=np.fft.rfft(x)/len(x);power=np.abs(spec)**2
    fundamental=2*np.abs(spec[hz]);total=power[1:].sum()
    other=max(0,total-power[hz]);allowed=np.arange(hz,len(power),hz)
    # Out-of-harmonic energy is an alias proxy. Folded components can coincide
    # with genuine harmonics; this metric cannot separate those contributions.
    alias=max(0,total-power[allowed].sum())
    db=lambda v:float(10*np.log10(max(1e-24,v)))
    rows.append(dict(r, rms_db=float(20*np.log10(np.sqrt(np.mean(x*x))+1e-24)),
        gain_db=float(20*np.log10(fundamental/level+1e-24)),
        thdn_db=db(other/max(power[hz],1e-24)),
        out_of_harmonic_db=db(alias/max(power[hz],1e-24)),
        dc=float(np.mean(x)),peak=float(np.max(np.abs(x)))))
with (root/'measurements.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)

fig,axes=plt.subplots(2,2,figsize=(12,7));fig.suptitle('ONYX 1.0.1 vs 1.0.2 — measured 48 kHz, 0.7 peak sine, 100%')
for k,ax in enumerate(axes.flat):
    for version,color in [('0','#b8795b'),('1','#168ba0')]:
        row=next(r for r in rows if int(r['knob'])==k and float(r['sr'])==48000 and float(r['amount'])==1 and float(r['level'])==.7 and float(r['hz'])==1000 and r['version']==version)
        x=np.fromfile(root/row['file'],dtype='<f4');s=np.fft.rfft(x)/len(x)
        ax.plot(np.arange(len(s)),20*np.log10(np.maximum(2*np.abs(s),1e-9)),color=color,label='1.0.1' if version=='0' else '1.0.2')
    ax.set(xlim=(0,20000),ylim=(-140,5),title=names[k],xlabel='Hz',ylabel='dBFS');ax.legend();ax.grid(alpha=.2)
fig.tight_layout();fig.savefig(root/'harmonic-spectra.png',dpi=150);plt.close(fig)

fig,axes=plt.subplots(1,3,figsize=(14,4))
data=np.genfromtxt(root/'transfer.csv',delimiter=',',names=True)
axes[0].plot(data['x'],data['old_tape'],label='1.0.1');axes[0].plot(data['x'],data['new_tape'],label='1.0.2');axes[0].set(title='Tape static transfer (before DC/filter)',xlabel='Input',ylabel='Output');axes[0].legend()
freq=np.geomspace(20,22000,500);a=1-np.exp(-2*np.pi*1600/48000);z=np.exp(-2j*np.pi*freq/48000);lp=a/(1-(1-a)*z)
for v,c in [('1.0.1',.30),('1.0.2',.25)]:axes[1].semilogx(freq,20*np.log10(np.abs(1+c*(1-lp))),label=v)
axes[1].set(title='SHINE linear response, 100%, 48 kHz',xlabel='Hz',ylabel='dB');axes[1].legend()
pulse=np.fromfile(root/'pulse-2.f32',dtype='<f4').reshape(-1,3)
t=np.arange(len(pulse))/48000
for i,label in enumerate(['Dry','1.0.1','1.0.2']):axes[2].plot(t,pulse[:,i],label=label,alpha=.7)
axes[2].set(xlim=(.099,.102),title='SMOOTHENING burst onset, 100%',xlabel='Seconds',ylabel='Amplitude');axes[2].legend()
for ax in axes:ax.grid(alpha=.2)
fig.tight_layout();fig.savefig(root/'transfer-response-transient.png',dpi=150);plt.close(fig)

audio=root/'level-matched-audio';audio.mkdir(exist_ok=True)
audio_stats=[]
for k in range(4):
    dry=np.fromfile(root/f'music-{k}-dry.f32',dtype='<f4').astype(float);target=np.sqrt(np.mean(dry*dry))
    for v in ['dry','old','new']:
        x=np.fromfile(root/f'music-{k}-{v}.f32',dtype='<f4').astype(float)
        rms=np.sqrt(np.mean(x*x));gain=target/max(rms,1e-12);x*=gain
        wavfile.write(audio/f'{names[k].lower().replace(" ","-")}-{v}.wav',48000,x.astype(np.float32))
        audio_stats.append(dict(knob=names[k],version=v,matching_gain_db=float(20*np.log10(gain)),peak=float(np.max(np.abs(x)))))
(root/'audio-levels.json').write_text(json.dumps(audio_stats,indent=2))
selected=[r for r in rows if float(r['sr'])==44100 and float(r['amount'])==1 and float(r['level'])==.7 and float(r['hz'])==8000]
print('44.1 kHz / 8 kHz tone / 0.7 peak / 100%:')
for r in selected:print(names[int(r['knob'])],r['version'],'THD+N',round(r['thdn_db'],2),'out-of-harmonic',round(r['out_of_harmonic_db'],2),'gain',round(r['gain_db'],2))
print('CPU:',(root/'cpu.csv').read_text())
print('Measured',len(rows),'tone cases; plots, metrics and 12 level-matched synthetic WAVs generated.')
