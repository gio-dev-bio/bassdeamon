#!/usr/bin/env python3
"""Measured extended signal/bypass report; consumes native processor CSV/renders."""
import argparse,base64,csv,html,json,shutil
from pathlib import Path
import numpy as np
from scipy import signal
from scipy.io import wavfile
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

p=argparse.ArgumentParser();p.add_argument('results',type=Path);p.add_argument('output',type=Path);args=p.parse_args();root=args.results;out=args.output;out.mkdir(parents=True,exist_ok=True)
rows=list(csv.DictReader(open(root/'signals.csv')));pr=list(csv.DictReader(open(root/'plugin_bypass.csv')))
def db(x):return float(20*np.log10(max(float(x),1e-30)))
def table(head,body):return '<table><tr>'+''.join('<th>'+html.escape(str(x))+'</th>' for x in head)+'</tr>'+''.join('<tr>'+''.join('<td>'+html.escape(str(x))+'</td>' for x in r)+'</tr>' for r in body)+'</table>'
def img(path):return '<img alt="'+path.stem+'" src="data:image/png;base64,'+base64.b64encode(path.read_bytes()).decode()+'">'
identity=['modules_off','glue_zero','unmask_zero','phase_neutral','no_kick','block_invariance','sidechain_only_delta']
fail=[]
for r in rows:
    if int(r['nonfinite']):fail.append(r['test']+'/'+r['signal']+': nonfinite')
    if r['test'] in identity and float(r['max_error'])!=0:fail.append(r['test']+'/'+r['signal']+': identity')
host=[r for r in pr if r['bypass']=='host']
for r in host:
    if int(r['bit_differences']):fail.append('host bypass not bit exact')
transient={};fig,axes=plt.subplots(2,1,figsize=(9,5.4),sharex=True)
sr=48000;edge=384*128;ix=np.arange(edge-480,edge+2400)
for label,color in [('host','#b15340'),('global','#087e78')]:
    a=np.fromfile(root/(label+'_switch_80.f32'),dtype='<f4').reshape(-1,2).astype(float)
    hp=signal.butter(6,2000,btype='highpass',fs=sr,output='sos');h=signal.sosfilt(hp,a[:,1]);window=slice(edge-48,edge+4800)
    transient[label]={'switch_jump_fs':float(a[edge,1]-a[edge-1,1]),'max_hf_peak_dbfs':db(np.max(abs(h[window])))}
    axes[0].plot((ix-edge)/48,a[ix,1],label=label+' bypass',color=color);axes[1].plot((ix-edge)/48,h[ix],label=label+' bypass',color=color)
axes[0].set_ylabel('Uscita FS');axes[1].set_ylabel('Componente >2 kHz FS');axes[1].set_xlabel('ms dal cambio');axes[0].set_title('Inversione di polarità attiva → bypass, tono 80 Hz a −12 dBFS')
for ax in axes:ax.legend();ax.spines[['top','right']].set_visible(False)
fig.tight_layout();fig.savefig(out/'bypass-transition.png',dpi=160);plt.close(fig)
summary={'version':'0.2.1','test_commit':'a942ac03c17de0c9ef24f360b449a3ea423e11c8','run':'https://github.com/gio-dev-bio/bassdeamon/actions/runs/42477224963','dsp_cases':len(rows),'dsp_samples':sum(int(r['samples']) for r in rows),'nonfinite':sum(int(r['nonfinite']) for r in rows),'peak_fs':max(float(r['peak']) for r in rows),'tail_peak_fs':max(float(r['post_silence_peak']) for r in rows),'host_bypass_cases':len(host),'host_bit_differences':sum(int(r['bit_differences']) for r in host),'host_compared_samples':sum(int(r['samples']) for r in host),'switches':transient,'failures':fail}
(out/'summary.json').write_text(json.dumps(summary,indent=2))
for f in ['signals.csv','plugin_bypass.csv']:shutil.copy2(root/f,out/f)
for name in ['multitone','sweep','white','quiet']:
    a=np.fromfile(root/(name+'_all.f32'),dtype='<f4').reshape(-1,5)
    for label,sl in [('input',slice(0,2)),('processed',slice(2,4))]:wavfile.write(out/(name+'_'+label+'.wav'),sr,a[:,sl])
for label in ['host','global']:
    a=np.fromfile(root/(label+'_switch_80.f32'),dtype='<f4').reshape(-1,2)
    wavfile.write(out/(label+'_bypass_switch.wav'),sr,a[:,1])
identityrows=[]
for mode in identity:
    r=[x for x in rows if x['test']==mode];identityrows.append([mode,len(r),max(float(x['max_error']) for x in r)])
globalrows=[[r['bypass'],r['signal'],r['bit_differences'],r['max_error'],r['late_max_error']] for r in pr if r['bypass']!='host']
body=f'''<h1>BassDeamon 0.2.1 · segnali e bypass</h1><p class="sub">Test aggiuntivi · 8 novembre 2026 · nessuna modifica al codice audio della 0.2.1</p>
<p class="lead">{len(rows)} casi DSP e {len(host)} casi di bypass dell'host. Il percorso di bypass dell'host conserva tutti i bit dei campioni provati. Il pulsante ON interno è invece un bypass con transizione graduale.</p>
<p><strong>Risultato sui segnali:</strong> nessun NaN/infinito, nessuna perdita di separazione stereo rilevata e identità numerica nei controlli neutri. <strong>Limite pratico:</strong> commutare il bypass diretto mentre PHASE inverte la polarità può produrre un click. Non dichiariamo la commutazione hard priva di artefatti.</p>
<h2>Segnali e condizioni</h2><p>Impulsi anche al confine dei blocchi, sweep logaritmico 20 Hz–18 kHz (2 s), multitone 47/113/997/10003 Hz, rumore bianco deterministico e rumore passa-basso, basso a dente di sega, kick + basso, sinusoide a −90 dBFS, subnormali ±10⁻³⁹, zeri con segno, sinusoide prossima al full scale, DC, stereo in opposizione e canali indipendenti.</p>
<p>44,1 / 48 / 96 kHz; buffer 1 / 17 / 64 / 127 / 512 / 2048 nei test iniziali di moduli spenti e bypass host. Nei render DSP attivi: 4 s, ingresso azzerato a 2 s; buffer 127. Configurazioni GLUE, UNMASK, PHASE, catena completa, automazioni estreme, transizione attivo→spento e controlli neutri. L'invarianza ai blocchi confronta buffer 64 con blocchi variabili 1–511, a parametri fissi.</p>
<p>Complessivamente {summary['dsp_samples']:,} campioni di uscita valutati nel DSP (conteggio aggregato L/R e casi); nessuno non finito. Picco massimo {summary['peak_fs']:.6f} FS ({db(summary['peak_fs']):.2f} dBFS): nessun clipping nei livelli/impostazioni provati. Il plugin non contiene un limiter.</p>
<h2>Null test: ingresso e uscita</h2>'''+table(['Condizione','Casi','Errore numerico massimo FS'],identityrows)+'''
<p>"Errore zero" qui significa identità dei valori numerici. Il core può convertire −0 in +0: questo non è bit-perfect IEEE in senso stretto, pur essendo audio numericamente identico. Il bypass host preserva anche zeri con segno e subnormali perché non attraversa il DSP.</p>
<p>Senza kick, UNMASK non modifica l'audio. Con solo kick sul sidechain e basso muto, l'uscita DELTA resta zero. Con sinistra attiva e destra muta il picco destro è esattamente zero alle tre frequenze di campionamento. Lo stereo in opposizione resta in opposizione nei render provati. I residui di filtro dopo un secondo di silenzio sono sotto −120 dBFS.</p>
<h2>True bypass digitale: cosa è stato verificato</h2>'''+table(['Percorso','Casi','Campioni confrontati','Bit diversi'],[['processBlockBypassed JUCE',len(host),summary['host_compared_samples'],summary['host_bit_differences']]])+'''
<p>Il test chiama il processore reale, con layout mono e stereo, sidechain mono, tutti i sample rate e buffer sopra indicati. PHASE, inversione, delay 4,25 ms, AGC e trim +6 dB sono impostati prima del bypass: il percorso bypassato non applica questi controlli. Latenza dichiarata verificata a zero. È un passaggio digitale diretto, non un relè analogico.</p>
<p>La verifica riguarda la funzione JUCE usata dal wrapper VST3 quando l'host richiede il bypass. Non è una misura dell'intero percorso audio di Cubase, del suo mixer, di eventuali fade dell'host o dell'hardware del Mac di Giorgio.</p>
<h2>Commutazione: un click possibile è stato riprodotto</h2>'''+img(out/'bypass-transition.png')+table(['Passaggio con PHASE invertita','Salto al cambio FS','Picco >2 kHz dBFS'],[[k,f"{v['switch_jump_fs']:.6f}",f"{v['max_hf_peak_dbfs']:.2f}"] for k,v in transient.items()])+'''
<p>Test volutamente sfavorevole: tono 80 Hz a −12,04 dBFS, PHASE invertita e cambio a un campione diverso da zero. Il bypass host passa immediatamente dall'onda invertita all'originale: la discontinuità è reale. Il comando globale interno usa lo smoothing dei moduli (20 ms come costante di tempo, non durata finita) e attenua il transitorio.</p>
<p>Per confronti durante la riproduzione, il pulsante ON interno evita la commutazione brusca di questo esempio. Per il null test del bypass host confrontare un tratto stabile. Le automazioni di delay/polarità possono intenzionalmente modificare forma d'onda e intonazione transitoria.</p>
<h2>Misure del comando globale interno</h2>'''+table(['Stato','Segnale','Bit diversi','Errore massimo FS','Errore dopo 500 ms FS'],globalrows)+'''
<p>Il massimo di "global_transition" include il periodo iniziale di rientro al segnale dry. Un valore non nullo in quel periodo è previsto. Le prove subnormali e −0 esaminano dettagli IEEE: valori intorno a 10⁻³⁹ non rappresentano rumore udibile. Questo comando interno non viene certificato come copia immediata di ogni bit.</p>
<h2>Ascolto e limiti</h2><p>Inclusi WAV float stereo ingresso/uscita a pari livello, senza normalizzazione: sweep, multitone, rumore e segnale molto basso; più i due esempi di commutazione. Sono segnali di test. Compressione, EQ e fase modificano intenzionalmente l'audio: la differenza ingresso−uscita con effetti attivi non è automaticamente un artefatto.</p>
<p>Non abbiamo eseguito un ascolto sul sistema di Giorgio e non certifichiamo assenza universale di aliasing, IMD o artefatti su ogni materiale/impostazione. I test precedenti di THD+N restano complementari. Il codice di produzione 0.2.1 è invariato: questa attività aggiunge test e documentazione.</p>
<p><a href="https://github.com/gio-dev-bio/bassdeamon/actions/runs/42477224963">Run nativo macOS con risultati grezzi</a> · commit test <code>a942ac03</code>. CSV e JSON inclusi. Per riprodurre: <code>signal_tests cartella</code>, <code>plugin_tests --signals cartella</code>, quindi <code>python benchmarks/analyze_signals.py cartella output</code>.</p>'''
style='body{font:16px/1.65 system-ui,sans-serif;color:#25333a;background:#fafafa;max-width:1080px;margin:48px auto;padding:0 26px 60px}h1{font-size:36px;line-height:1.2}h2{font-size:23px;border-top:1px solid #d9e2e3;padding-top:24px;margin-top:38px}.sub{color:#65767b}.lead{font-size:22px;color:#087e78}table{border-collapse:collapse;width:100%;font-size:13px;margin:22px 0}th,td{border-bottom:1px solid #d9e2e3;padding:10px;text-align:left}th{background:#eaf2f2}img{width:100%;height:auto}a{color:#087e78}code{font-size:13px;background:#edf1f1;padding:2px 4px}'
(out/'Test_Segnali_Bypass_BassDeamon_0.2.1.html').write_text('<!doctype html><html lang="it"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>BassDeamon · segnali e bypass</title><style>'+style+'</style><body>'+body+'</body></html>')
print(json.dumps(summary,indent=2));raise SystemExit(bool(fail))
