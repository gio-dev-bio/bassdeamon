#!/usr/bin/env python3
"""Metrics from coherent synthetic fixtures. Requires numpy and scipy; no listening claim."""
import argparse, csv, json, math, pathlib, platform, subprocess
import numpy as np
from scipy import signal

def db(x): return float(20*np.log10(max(float(x),1e-20)))
def run(root):
    rows=[]; failures=[]; reference=root/'reference'
    manifest=list(csv.DictReader(open(root/'fixtures.csv')))
    for row in manifest:
        name=row['file'];a=np.fromfile(root/(name+'.f32'),dtype='<f4').reshape(-1,3)
        x=a[:,0].astype(float);y=a[:,1].astype(float);sr=int(row['sr']);f=float(row['frequency'])
        r={'file':name,'mode':row['mode'],'peak_dbfs':db(np.max(np.abs(y))),'nonfinite':int(np.count_nonzero(~np.isfinite(y))), 'dc_last_2s_dbfs':db(abs(np.mean(y[-2*sr:])))}
        if r['nonfinite']:failures.append(name+': non-finite output')
        if name.startswith('tone_') or name.startswith('quiet_'):
            steady=y[-2*sr:];t=np.arange(len(steady))/sr
            basis=np.column_stack([np.sin(2*np.pi*f*t),np.cos(2*np.pi*f*t),np.ones(len(t))])
            coefficients=np.linalg.lstsq(basis,steady,rcond=None)[0]
            fund=np.hypot(*coefficients[:2])/np.sqrt(2);res=steady-basis@coefficients
            r['thdn_db']=db(np.sqrt(np.mean(res*res))/max(fund,1e-20))
            harmonics=[]
            for h in range(2,min(21,int(sr/(2*f)))):
                re=2*np.mean(steady*np.cos(2*np.pi*f*h*t));im=2*np.mean(steady*np.sin(2*np.pi*f*h*t));harmonics.append((re*re+im*im)/2)
            r['thd_db']=db(np.sqrt(sum(harmonics))/max(fund,1e-20))
            r['gain_db']=db(fund/(float(row['amplitude'])/np.sqrt(2)))
            if row['mode']=='bypass' and not np.array_equal(a[:,0],a[:,1]):failures.append(name+': bypass not exact')
            limit=-135 if row['mode']=='phase' else -60 if row['mode']=='glue' else -85 if name.startswith('quiet') else -100
            if r['thdn_db']>limit:failures.append(name+': THD+N above regression threshold '+str(limit))
        if name=='silence':
            r['exact_silence']=bool(np.all(y==0))
            if not r['exact_silence']:failures.append('silence: nonzero output')
        if name=='tail':
            r['tail_after_5s_peak_dbfs']=db(np.max(np.abs(y[5*sr:])))
            if r['tail_after_5s_peak_dbfs']>-120:failures.append('tail: residual after 3 seconds of silence')
        if name in ('width_toggle','bypass_toggle','automation_phase'):
            hp=signal.butter(6,2000,btype='highpass',fs=sr,output='sos')
            h=signal.sosfilt(hp,y)[sr:]
            r['hf_2khz_peak_dbfs']=db(np.max(np.abs(h)));r['hf_2khz_rms_dbfs']=db(np.sqrt(np.mean(h*h)))
            if reference.exists():
                b=np.fromfile(reference/(name+'.f32'),dtype='<f4').reshape(-1,3)
                h0=signal.sosfilt(hp,b[:,1].astype(float))[sr:]
                r['reference_hf_peak_dbfs']=db(np.max(np.abs(h0)))
                r['hf_peak_improvement_db']=r['reference_hf_peak_dbfs']-r['hf_2khz_peak_dbfs']
                if name=='width_toggle' and r['hf_peak_improvement_db']<6:failures.append('width automation: insufficient transient reduction')
        rows.append(r)
    cpu=list(csv.DictReader(open(root/'cpu.csv')))
    for r in cpu:
        if int(r['callback_allocations'])!=0:failures.append('C++ callback allocation: '+r['mode'])
    null=list(csv.DictReader(open(root/'null_comparison.csv')))
    for r in null:
        if int(r['nonfinite']):failures.append('null nonfinite: '+r['mode'])
        # Width smoothing intentionally changes the automation fixture; all other cases should null closely.
        if r['mode']!='automation' and float(r['max_absolute_error'])>1e-6:failures.append('null error: '+r['mode'])
    result={'machine':platform.machine(),'platform':platform.platform(),'baseline_commit':'e6d48b7fa76979da31214eca03d4190fdef3578e',
            'cpu_method':'Stereo core DSP only; 3 repeats x 0.6 s, 30 warmup blocks. Wall-clock timings; shared non-realtime runner. No host/GUI load included.',
            'allocation_scope':'C++ new/new[] including aligned forms in the core callback; not all operating-system malloc calls.',
            'audio_method':'48 kHz float output. Coherent 40/80/160 Hz tones, 10 s, last 2 s analysed; least-squares fundamental/DC removal. Dynamic fixtures are NOT evaluated as stationary THD+N.',
            'transient_method':'Extreme width 0/100% at 4 changes/s; 6th order Butterworth high-pass 2 kHz, ignore first 1 s. Difference is not a perceptual audibility certificate.',
            'failures':failures,'audio':rows,'null':null}
    (root/'audio_metrics.json').write_text(json.dumps(result,indent=2))
    print(json.dumps({'failures':failures,'width':[r for r in rows if r['file']=='width_toggle'],'silence':[r for r in rows if r['file']=='silence']}))
    return 1 if failures else 0
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('directory',type=pathlib.Path);args=p.parse_args();raise SystemExit(run(args.directory))
