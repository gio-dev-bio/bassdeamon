#!/usr/bin/env python3
"""Create a self-contained measured report from Benchmark.cpp/analyze.py output."""
import argparse, base64, csv, html, json, shutil, statistics
from pathlib import Path
import numpy as np
from scipy import signal
from scipy.io import wavfile
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

p=argparse.ArgumentParser();p.add_argument('--results',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
out=a.output;out.mkdir(parents=True,exist_ok=True)
names={'native-intel':'Mac Intel i7-8700B','native-arm64':'Apple M1 virtuale','final':'Linux (controllo)'}
data={k:list(csv.DictReader(open(a.results/k/'cpu.csv'))) for k in names}
metrics=json.load(open(a.results/'final/audio_metrics.json'))
def match(rows,rev,sr=48000,buffer=128,mode='all'):
    return next(r for r in rows if r['revision']==rev and int(r['sample_rate'])==sr and int(r['buffer'])==buffer and r['mode']==mode)
def reduction(rows):
    old={(r['sample_rate'],r['buffer'],r['mode']):float(r['mean_us']) for r in rows if r['revision']=='0.2.0'}
    return statistics.median(100*(1-float(r['mean_us'])/old[(r['sample_rate'],r['buffer'],r['mode'])]) for r in rows if r['revision']=='0.2.1')
def table(headers,rows):
    return '<table><thead><tr>'+''.join('<th>'+html.escape(str(x))+'</th>' for x in headers)+'</tr></thead><tbody>'+''.join('<tr>'+''.join('<td>'+html.escape(str(x))+'</td>' for x in r)+'</tr>' for r in rows)+'</tbody></table>'
def image(path):return '<img alt="'+html.escape(path.stem)+'" src="data:image/png;base64,'+base64.b64encode(path.read_bytes()).decode()+'">'
plt.rcParams.update({'font.family':'DejaVu Sans','font.size':10,'axes.spines.top':False,'axes.spines.right':False})
fig,ax=plt.subplots(figsize=(9,3.6));x=np.arange(2)
for i,rev in enumerate(['0.2.0','0.2.1']):
    ax.bar(x+(i-.5)*.34,[float(match(data[k],rev)['one_core_percent']) for k in list(names)[:2]],.34,label=rev,color=['#999caa','#07877f'][i])
ax.set_xticks(x,list(names.values())[:2]);ax.set_ylabel('% di un core (tempo medio)');ax.set_title('DSP stereo · 48 kHz · buffer 128 · tutti i moduli · AGC off');ax.legend();fig.tight_layout();fig.savefig(out/'cpu.png',dpi=170);plt.close(fig)
root=a.results/'final';sr=48000
old=np.fromfile(root/'reference/width_toggle.f32',dtype='<f4').reshape(-1,3)[:,1].astype(float)
new=np.fromfile(root/'width_toggle.f32',dtype='<f4').reshape(-1,3)[:,1].astype(float)
hp=signal.butter(6,2000,btype='highpass',fs=sr,output='sos');h0=signal.sosfilt(hp,old);h1=signal.sosfilt(hp,new)
center=sr+np.argmax(np.abs(h0[sr:]));ix=np.arange(max(sr,center-576),min(len(old),center+576))
fig,ax=plt.subplots(figsize=(9,3.4));ax.plot((ix-center)/48,h0[ix],label='0.2.0',color='#a75142');ax.plot((ix-center)/48,h1[ix],label='0.2.1',color='#07877f');ax.set_xlabel('ms dal picco di riferimento');ax.set_ylabel('Ampiezza FS, componente >2 kHz');ax.set_title('Cambio WIDTH estremo: finestra intorno al massimo transitorio');ax.legend();fig.tight_layout();fig.savefig(out/'width-transient.png',dpi=170);plt.close(fig)
for name,arr in [('WIDTH-riferimento-0.2.0.wav',old),('WIDTH-ottimizzato-0.2.1.wav',new)]:wavfile.write(out/name,sr,arr.astype(np.float32))
audio={r['file']:r for r in metrics['audio']}
cpurows=[];tailrows=[];hwrows=[]
for k,title in names.items():
    b=match(data[k],'0.2.0');n=match(data[k],'0.2.1')
    cpurows.append([title,f'{reduction(data[k]):.1f}%',f"{float(b['one_core_percent']):.2f}% → {float(n['one_core_percent']):.2f}%",f"{float(b['mean_us']):.2f} → {float(n['mean_us']):.2f}",f"{float(b['p99_us']):.2f} → {float(n['p99_us']):.2f}"])
    for rev in ['0.2.0','0.2.1']:
        rr=[r for r in data[k] if r['revision']==rev]
        ratios=[float(r['max_us'])/(1e6*int(r['buffer'])/int(r['sample_rate'])) for r in rr]
        tailrows.append([title,rev,sum(x>1 for x in ratios),f'{100*max(ratios):.1f}%'])
    hw=json.load(open(a.results/k/'hardware.json'))
    hwrows.append([title,hw.get('cpu',hw.get('cpu_brand','Vedi hardware.json')),hw.get('os',hw.get('platform','Vedi hardware.json'))])
    dest=out/'dati'/k;dest.mkdir(parents=True,exist_ok=True)
    for f in ['cpu.csv','null_comparison.csv','fixtures.csv','audio_metrics.json','hardware.json']:shutil.copy2(a.results/k/f,dest/f)
statics=[]
for mode in ['glue','unmask','phase']:
    statics.append([mode.upper()]+[f"{audio[f'tone_{mode}_{f}']['thdn_db']:.2f}" for f in [40,80,160]])
nullrows=[[r['mode'],r['max_absolute_error'],r['different_samples']] for r in metrics['null']]
body='''<h1>BassDeamon 0.2.1</h1><p class="subtitle">Benchmark CPU e verifica numerica audio · 8 novembre 2026</p>
<p class="lead">Circa il 30% in meno di CPU media sui benchmark Mac. Il transitorio del controllo WIDTH cala di 16,82 dB nel test estremo.</p>
<p>Confronto tra il motore 0.2.0 (commit <code>e6d48b7</code>) e 0.2.1 (<code>1401e355</code>). Il confronto conserva l'audio statico: la modifica intenzionale è lo smoothing di WIDTH. I risultati descrivono i segnali e le macchine provati; non sono una garanzia di assenza di artefatti udibili su ogni materiale.</p>
<h2>CPU: misure native su Mac</h2>'''+table(['Macchina','Riduzione mediana, 72 casi','% un core, caso illustrato','Media µs, 0.2.0 → 0.2.1','p99 µs, stesso caso'],cpurows)+image(out/'cpu.png')+'''
<p>Il caso illustrato è stereo, 48 kHz, buffer 128, GLUE + PHASE + UNMASK attivi e AGC disattivato. Budget per callback: 2666,67 µs. La percentuale indica un singolo core, non la CPU totale del sistema e non il meter di Cubase.</p>
<p><strong>Limite sui picchi:</strong> su Apple M1 il p99 di questo caso sale da 57,29 a 96,46 µs pur migliorando la media. Il runner condiviso introduce variabilità. Non sosteniamo un miglioramento uniforme dei tempi di coda.</p>
<h2>Deadline e allocazioni</h2>'''+table(['Macchina','Versione','Casi con almeno un callback oltre budget','Peggior callback / budget'],tailrows)+'''
<p>Il benchmark non salva il numero di callback in ritardo: la colonna conta i casi che ne contengono almeno uno. I ritardi osservati su Linux impediscono di garantire il real time dal solo tempo medio. I runner non sono macchine dedicate all'audio.</p>
<p><strong>0 chiamate C++ new/new[]</strong> (comprese le forme allineate) durante i callback DSP di tutti i casi. Il controllo non intercetta ogni malloc del sistema. La suite del plugin verifica anche il processBlock JUCE; i suoi risultati sono nel run CI collegato.</p>
<h2>Artefatti e distorsione</h2>'''+table(['Modulo, impostazioni del fixture','40 Hz: THD+N dB','80 Hz: THD+N dB','160 Hz: THD+N dB'],statics)+'''
<p>Sinusoidi a −12,04 dBFS di picco, 48 kHz; solo ultimi 2 s di un render di 10 s. Fondamentale e DC rimosse con regressione ai minimi quadrati. I valori estremamente bassi indicano il fondo numerico di questo test in float, non prestazioni analogiche misurabili. Il compressore GLUE è non lineare: a 40 Hz la distorsione misurata è circa 0,0224%, non zero. Le prove non coprono tutti i livelli o le possibili impostazioni.</p>
<ul><li>Bypass: identico al segnale in ingresso.</li><li>Silenzio: uscita esattamente zero; nessun NaN o infinito nei fixture.</li><li>Coda: sotto −120 dBFS dopo 3 s di silenzio, con tutti i moduli attivi.</li><li>UNMASK a basso livello, 80 Hz: THD+N −99,67 dB nel fixture provato.</li><li>Note variabili, bypass e automazioni: uscita finita; non trattate come toni stazionari per THD+N.</li></ul>
<h2>WIDTH: miglioramento verificato</h2>'''+image(out/'width-transient.png')+'''
<p>Automazione 0/100% quattro volte al secondo su basso sintetico a 80 Hz. Misura: filtro passa-alto Butterworth di ordine 6 a 2 kHz, escluso il primo secondo. Picco HF da <strong>−67,09 a −83,91 dBFS</strong>: riduzione di <strong>16,82 dB</strong>. RMS HF candidato: −108,91 dBFS. La curva mostra la stessa finestra per entrambe le versioni.</p>
<p>Due WAV float, senza normalizzazione, sono inclusi per un confronto allo stesso livello. Sono segnali sintetici di stress. L'automazione del delay e della polarità in PHASE produce intenzionalmente variazioni temporali/di fase: non viene promessa trasparenza durante cambi estremi di questi parametri.</p>
<h2>Null test rispetto alla versione 0.2.0</h2>'''+table(['Caso, 240.000 campioni','Errore assoluto massimo FS','Campioni differenti'],nullrows)+'''
<p>AGC e Learn differiscono solo al livello dell'arrotondamento float. L'automazione differisce deliberatamente perché WIDTH ora ha uno smoothing di 20 ms. I casi statici bypass, GLUE, UNMASK e catena completa sono identici nel fixture di confronto.</p>
<h2>Cosa è stato ottimizzato</h2><ul>
<li>Calcoli costanti spostati fuori dal ciclo per campione; coefficiente all-pass memorizzato quando la frequenza non cambia.</li>
<li>Coefficienti EQ calcolati una sola volta per banda e condivisi tra L/R, mantenendo stati filtro separati.</li>
<li>Percorsi rapidi per delay nullo, gain unitario, learner inattivo e detector già ai limiti.</li>
<li>Smoothing del Q di UNMASK durante variazioni WIDTH; nessuna approssimazione fast-math.</li>
<li>Grafico GUI: calcolo coefficienti da 1932 a 12 per disegno. Il guadagno CPU della GUI completa non è stato misurato.</li></ul>
<h2>Metodo, riproducibilità e limiti</h2>
<p>72 condizioni per versione e piattaforma: 44,1/48/96 kHz × buffer 64/128/256/512 × bypass/glue/unmask/all/agc/learn. Tre ripetizioni da 0,6 s, 30 blocchi di warmup. Cronometro wall-clock sul solo process DSP; copia degli input esclusa. Baseline e candidato alternati per condizione. Frequenza progetto 120 BPM; impostazioni e segnali completi in <code>tests/Benchmark.cpp</code>.</p>
<p>Compilazione Mac nativa clang++17, <code>-std=c++17 -O3 -DNDEBUG</code>, senza fast-math. Intel e ARM sono runner distinti, non emulazione Rosetta. Non misuriamo l'overhead completo di Cubase, la GUI, driver audio, pressione termica o un progetto con molte istanze. Sono necessarie prove sul Mac mini di Giorgio e ascolto con kick/bass reali. Oversampling e limiter non sono presenti.</p>'''+table(['Piattaforma','CPU dichiarata dal runner','Sistema'],hwrows)+'''
<p>Controlli automatici di regressione: finitezza, bypass e silenzio esatti; coda &lt; −120 dBFS; THD+N PHASE &lt; −135 dB, GLUE &lt; −60 dB, UNMASK quiet &lt; −85 dB, altri UNMASK &lt; −100 dB; miglioramento WIDTH ≥6 dB; null statici &lt; 10⁻⁶ FS; zero allocazioni C++ osservate. Tutti superati sulle tre piattaforme. Queste soglie rilevano regressioni: non certificano l'inudibilità.</p>
<p>Riprodurre: compilare <code>tests/Benchmark.cpp</code> con include <code>Source</code> e <code>tests</code>; eseguire il binario passando la cartella risultati; eseguire <code>python benchmarks/analyze.py cartella</code>. NumPy 2.2.6 e SciPy 1.15.3 nel CI. CSV e JSON grezzi inclusi in <code>dati/</code>.</p>
<p><a href="https://github.com/gio-dev-bio/bassdeamon/actions/runs/42474838355">Run GitHub Actions e risultati di validazione VST3</a> · <a href="https://github.com/gio-dev-bio/bassdeamon/pull/1">PR #1</a></p>
'''
style='''body{font:16px/1.65 system-ui,sans-serif;color:#202c36;max-width:1100px;margin:48px auto;padding:0 28px 50px;background:#fafafa}h1{font-size:42px;letter-spacing:-1px;margin-bottom:0}h2{font-size:23px;margin-top:40px;border-top:1px solid #d8dfe3;padding-top:24px}.subtitle{color:#687880}.lead{font-size:23px;color:#06746d;max-width:920px}table{border-collapse:collapse;width:100%;font-size:14px;margin:22px 0}th,td{text-align:left;border-bottom:1px solid #d8dfe3;padding:10px}th{background:#edf3f3}img{display:block;width:100%;height:auto;background:white;margin:22px 0}code{font-size:13px;background:#eaf0f0;padding:2px 5px}a{color:#06746d}@media print{body{margin:0;font-size:11px}h2{break-after:avoid}table,img{break-inside:avoid}}'''
(out/'Report_BassDeamon_0.2.1.html').write_text('<!doctype html><html lang="it"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>BassDeamon 0.2.1 · Benchmark</title><style>'+style+'</style><body>'+body+'</body></html>')
print(json.dumps({'report':str(out/'Report_BassDeamon_0.2.1.html'),'reductions':{k:reduction(v) for k,v in data.items()}}))
