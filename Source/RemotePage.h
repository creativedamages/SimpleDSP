#pragma once

// Single-file mobile controller. All resources are served locally by the Windows app.
static constexpr const char* kRemotePage = R"DSP_HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>HT DSP</title>
<meta name="theme-color" content="#111821"><meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-title" content="HT DSP">
<link rel="manifest" href="/manifest.webmanifest"><style>
:root{color-scheme:dark;--bg:#111821;--panel:#1d2734;--card:#253140;--muted:#a0b3c7;--accent:#65cad8;--input:#65cad8;--output:#ff69b4;--eq-curve:#f6cd70;--border:#34485d}
*{box-sizing:border-box}body{font:15px system-ui,-apple-system,'Segoe UI',sans-serif;background:var(--bg);color:#edf5fb;margin:0;padding:16px max(12px,env(safe-area-inset-right)) 45px}header{max-width:1160px;margin:auto;display:flex;gap:12px;justify-content:space-between;align-items:center;flex-wrap:wrap}h1{font-size:21px;margin:0}h2{font-size:19px;margin:0 0 14px;color:white}.muted,small{color:var(--muted)}.status{font-size:12px;color:var(--accent)}.bar{max-width:1160px;margin:14px auto;display:flex;flex-wrap:wrap;gap:8px;align-items:center}.bar input{width:145px}.channels{max-width:1160px;margin:auto;display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:15px}.panel{background:var(--panel);border:1px solid var(--border);border-radius:15px;padding:16px;min-width:0}.control{display:grid;grid-template-columns:98px 1fr 65px;gap:7px;align-items:center;margin:9px 0}.control label{font-size:13px;color:var(--muted)}.control output{text-align:right;font-variant-numeric:tabular-nums;font-size:12px;color:#e1eef4}input[type=range]{width:100%;accent-color:var(--accent);min-width:0}input[type=text],select{background:#111c28;color:white;border:1px solid #476073;border-radius:8px;padding:9px;min-width:0;width:100%}button{background:var(--card);color:white;border:1px solid #547081;border-radius:8px;padding:10px;cursor:pointer}button.primary{background:#287d91;border-color:#5bbbcf}button:active{filter:brightness(1.3)}.routing{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:12px}.routing label{font-size:12px;color:var(--muted)}.checkline{display:flex;align-items:center;gap:7px;font-size:13px;margin:12px 0}.checkline input{accent-color:var(--accent)}.filter{background:var(--card);border-radius:9px;padding:10px;margin:11px 0}.filter .head{display:flex;justify-content:space-between;gap:8px;align-items:center}.filter select{width:104px;padding:6px}.band{background:var(--card);border-radius:8px;padding:9px;margin:9px 0}.band strong{font-size:12px;color:var(--accent)}.graph{width:100%;height:195px;display:block;background:#101923;border-radius:8px;touch-action:none;margin-bottom:13px}.tabs{display:none}.group{display:grid;grid-template-columns:1fr 1fr;gap:10px}.toast{position:fixed;bottom:10px;left:12px;right:12px;max-width:480px;margin:auto;background:#153644;padding:12px;border-radius:9px;text-align:center;display:none}#pin{display:none;max-width:400px;margin:10vh auto;padding:25px;background:var(--panel);border-radius:14px}#pin input{margin:12px 0;font-size:20px;letter-spacing:4px;text-align:center} .startup-status{max-width:1160px;margin:0 auto 15px;color:var(--accent);font-size:13px;min-height:18px}
.channel-title{display:flex;align-items:center;gap:12px;margin-bottom:14px}.channel-title h2{white-space:nowrap;margin:0}.channel-title input{min-width:0;flex:1;font-weight:600;font-size:17px}.channel-title .mute{white-space:nowrap;min-width:93px;padding:10px 8px;border:1px solid #a94f69;color:#ffb5d7;font-weight:700;font-size:12px;background:#293947}.channel-title .mute.muted{background:#ab2348;border-color:#ff719c;color:#fff}
.meters{display:grid;gap:7px;background:#111c28;border-radius:9px;padding:10px 10px 8px;margin-bottom:11px}.meterheading{color:var(--muted);font-size:10px;letter-spacing:.08em}.meterline{display:grid;grid-template-columns:38px minmax(0,1fr) 62px 44px;gap:8px;align-items:center}.meterlabel{font-size:11px;color:var(--muted)}.metertrack{height:13px;position:relative;border-radius:4px;background:#293745;overflow:visible}.meterrms{height:100%;width:0;background:var(--input);border-radius:3px;opacity:.65}.meterline.out .meterrms{background:var(--output)}.meterpeak{position:absolute;top:-2px;left:0%;width:3px;height:17px;border-radius:2px;background:#a5f0fb}.meterline.out .meterpeak{background:#ffa6d1}.meterline.clip .meterpeak{background:#ff5f6f}.clipbtn{height:21px;padding:0 3px;border-radius:4px;border:1px solid #3f5262;background:#293745;color:#8599ab;font-size:10px;font-weight:700}.meterline.clip .clipbtn{background:#db3159;border-color:#ff5b82;color:#fff}.meterline.in .meterlabel{color:var(--input)}.meterline.out .meterlabel{color:var(--output)}.metervalue{font-size:12px;text-align:right;font-variant-numeric:tabular-nums}.graphlegend{display:flex;justify-content:flex-end;gap:16px;align-items:center;font-size:11px;color:#b0c4d3;margin:-6px 3px 12px}.graphlegend .eq-dot{color:var(--eq-curve)}.graphlegend .rta-dot{color:var(--output)}.graphlegend .input-dot{color:var(--input)}.fold{margin:9px 0 14px;border:1px solid var(--border);border-radius:9px;overflow:hidden}.fold summary{cursor:pointer;padding:13px 12px;color:var(--accent);font-weight:650;font-size:14px;background:var(--card);list-style-position:inside}.fold[open]{padding-bottom:8px}.fold[open]>summary{border-bottom:1px solid var(--border);margin-bottom:8px}.fold>.filter,.fold>.band{margin:8px 10px}
@media(max-width:790px){.channels{display:block}.panel{margin-bottom:14px}.tabs{display:flex;max-width:1160px;margin:0 auto 10px;gap:8px}.tabs button{flex:1}.panel.hidden{display:none}.bar{gap:7px}.bar input{flex:1}.routing{gap:7px}h1{font-size:19px}}
 .channel-select{max-width:840px;margin:0 auto 12px;display:flex;align-items:center;gap:8px}
 .channels{display:block;max-width:840px}
</style></head><body>
<header><div><h1>HT DSP</h1></div><span id="status" class="status">Connecting…</span></header>
<div id="pin"><h2>Enter remote PIN</h2><p class="muted">Enter the six-digit PIN shown on the Windows computer.</p><input type="text" id="pinvalue" inputmode="numeric" maxlength="6" placeholder="000000"><button class="primary" onclick="connectPin()">Connect</button></div>
<div id="app" style="display:none"><div class="bar"><input id="presetname" type="text" placeholder="Preset name" maxlength="60"><button onclick="savePreset()">Save preset</button><select id="presets" aria-label="Saved presets"><option>Saved presets</option></select><button onclick="loadPreset()">Load preset</button><button class="primary" onclick="setStartupPreset()">Set as load-at-launch</button></div><div id="startupStatus" class="startup-status" role="status">Startup preset: not configured</div>
<section class="panel" style="max-width:840px;margin:0 auto 16px">
<label class="checkline"><input id="generatorEnabled" type="checkbox" onchange="setGenerator('enabled',this.checked)">Test generator ON</label>
<select id="generatorMode" aria-label="Test signal" onchange="setGenerator('mode',Number(this.value))"><option value="0">Pink noise</option><option value="1">Sine oscillator</option><option value="2">1 kHz tone</option></select>
<div class="control"><label for="generatorLevel">Global level</label><input id="generatorLevel" type="range" min="-90" max="0" step="0.1" value="-40" oninput="this.nextElementSibling.textContent=this.value+' dBFS';setGenerator('levelDb',Number(this.value))"><output>-40 dBFS</output></div>
<div class="control"><label for="generatorFrequency">Frequency</label><input id="generatorFrequency" type="number" min="20" max="20000" step="1" value="1000" onchange="setGenerator('frequency',Number(this.value))"><output>Hz</output></div>
</section><div class="channel-select"><label for="channelPicker">Channel</label><select id="channelPicker" onchange="selectChannel(Number(this.value))"></select></div><main class="channels" id="channels"></main></div><div class="toast" id="toast"></div>
<script>
)DSP_HTML"
R"DSP_HTML(let pin=localStorage.getItem('htDspPin')||'',model=null,telemetry=null,selectedTab=0,busy=false,editingUntil=0,telemetryBusy=false;
const foldState={};
const $=id=>document.getElementById(id),esc=s=>String(s??'').replace(/[&<>"']/g,x=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[x]));
function notice(s){$('toast').textContent=s;$('toast').style.display='block';setTimeout(()=>$('toast').style.display='none',2800)}
async function api(path,method='GET',data){const r=await fetch(path,{method,headers:{'X-DSP-PIN':pin,'Content-Type':'application/json'},body:data?JSON.stringify(data):undefined,cache:'no-store'});if(r.status===401){$('app').style.display='none';$('pin').style.display='block';throw Error('Enter PIN from the Windows application')}if(!r.ok)throw Error((await r.text()).slice(0,120));return r.json()}
function connectPin(){pin=$('pinvalue').value.trim();localStorage.setItem('htDspPin',pin);refresh(true)}
async function setGenerator(key,value){
  if(typeof value==='number'&&!Number.isFinite(value))return;
  editingUntil=Date.now()+1100;
  try{await api('/api/set','POST',{generator:true,key,value})}
  catch(e){notice(e.message)}
}
async function engageTestInput(channel,control){
  const enabled=control.checked;
  control.checked=Boolean(model.channels[channel].testInput);
  control.disabled=true;
  try{
    const latest=await api('/api/state');
    let confirmed=false;
    if(enabled&&latest.generator.enabled){
      confirmed=confirm('Proceed with caution. The test generator is ON. This will replace CH '+(channel+1)+' input with the active test signal at '+latest.generator.levelDb+' dBFS.');
      if(!confirmed)return;
    }
    await api('/api/set','POST',{channel,key:'testInput',value:enabled,confirmed});
    setTimeout(()=>refresh(true),150);
  }catch(e){notice(e.message)}
  finally{control.disabled=false}
}
function syncGenerator(g){
  if(!g)return;
  $('generatorEnabled').checked=g.enabled;
  $('generatorMode').value=String(g.mode);
  if(document.activeElement!==$('generatorLevel')){
    $('generatorLevel').value=g.levelDb;
    $('generatorLevel').nextElementSibling.textContent=Number(g.levelDb).toFixed(1)+' dBFS';
  }
  if(document.activeElement!==$('generatorFrequency'))$('generatorFrequency').value=g.frequency;
  $('generatorFrequency').disabled=g.mode!==1;
}
function selectChannel(index){
  if(!model?.channels[index])return;
  selectedTab=index;
  renderChannel();
  refreshTelemetry();
}
function renderChannel(){
  document.querySelectorAll('details[data-fold]').forEach(el=>{foldState[el.dataset.fold]=el.open});
  const c=model.channels[selectedTab];
  $('channels').innerHTML=channel(c,selectedTab,model);
  handlers();
  const live=telemetry?.channel===selectedTab?telemetry:null;
  graph(document.querySelector('[data-graph]'),c,live);
  paintMeters();
}

const format=(v,d=1)=>Number(v).toFixed(d);function range(ch,key,label,val,min,max,step,unit,band){const idx=band===undefined?'':` data-band="${band}"`,log=key==='frequencyHz'||key.endsWith('.frequency');return `<div class="control"><label>${label}</label><input type="range" data-ch="${ch}" data-key="${key}"${idx} data-log="${log?'1':'0'}" min="${log?0:min}" max="${log?1000:max}" step="${log?1:step}" value="${log?Math.round(Math.log(Math.max(20,val)/20)/Math.log(1000)*1000):val}"><output>${format(val,step<.1?2:step<1?1:0)} ${unit}</output></div>`}
function cutoff(ch,key,caption,filter){return `<div class="filter"><div class="head"><label class="checkline"><input type="checkbox" data-ch="${ch}" data-key="${key}.enabled" ${filter.enabled?'checked':''}>${caption}</label><select data-ch="${ch}" data-key="${key}.slope"><option value="12" ${filter.slope==12?'selected':''}>12 dB/oct</option><option value="24" ${filter.slope==24?'selected':''}>24 dB/oct</option></select></div>${range(ch,key+'.frequency','Frequency',filter.frequency,20,20000,1,'Hz')}</div>`}
function meterHtml(i){return `<div class="meters"><div class="meterheading">SIGNAL · PEAK dBFS / RMS FILL</div><div class="meterline in"><span class="meterlabel">IN</span><div class="metertrack"><div class="meterrms" data-rms="${i}-in"></div><div class="meterpeak" data-peak="${i}-in"></div></div><span class="metervalue" data-meter-value="${i}-in">-96.0</span><button type="button" class="clipbtn" onclick="clearClip(${i},'in')" aria-label="Clear input clip indicator">CLIP</button></div><div class="meterline out"><span class="meterlabel">OUT</span><div class="metertrack"><div class="meterrms" data-rms="${i}-out"></div><div class="meterpeak" data-peak="${i}-out"></div></div><span class="metervalue" data-meter-value="${i}-out">-96.0</span><button type="button" class="clipbtn" onclick="clearClip(${i},'out')" aria-label="Clear output clip indicator">CLIP</button></div></div>`}
function channel(c,i,m){return `<section class="panel" data-channel="${i}"><div class="channel-title"><h2>CH ${i+1}</h2><input type="text" maxlength="40" data-ch="${i}" data-key="name" aria-label="CH ${i+1} name" value="${esc(c.name)}"><button type="button" class="mute ${c.outputMuted?'muted':''}" data-mute="${i}" aria-pressed="${c.outputMuted?'true':'false'}" onclick="toggleMute(${i})">${c.outputMuted?'MUTED':'MUTE OUT'}</button></div><div class="routing"><div><label>Input</label><select data-ch="${i}" data-key="inputRoute">${options(m.inputs,c.inputRoute)}</select></div><div><label>Output</label><select data-ch="${i}" data-key="outputRoute">${options(m.outputs,c.outputRoute)}</select></div></div><label class="checkline"><input type="checkbox" data-test-input="${i}" ${c.testInput?'checked':''} onchange="engageTestInput(${i},this)">Use test signal</label>${meterHtml(i)}<canvas class="graph" data-graph="${i}" width="640" height="350"></canvas><div class="graphlegend"><span class="eq-dot">● EQ curve</span><span class="input-dot">● Input RTA</span><span class="rta-dot">● Output RTA · dBFS</span></div><details class="fold" data-fold="${i}-level" ${foldState[i+'-level']?'open':''}><summary>Level and delay</summary>${range(i,'levelDb','Level',c.levelDb,-90,18,.1,'dB')}${range(i,'delayMs','Delay',c.delayMs,0,500,.01,'ms')}</details><label class="checkline"><input type="checkbox" data-ch="${i}" data-key="invertPolarity" ${c.invertPolarity?'checked':''}>Invert polarity (180 deg)</label><details class="fold" data-fold="${i}-filters" ${foldState[i+'-filters']?'open':''}><summary>Filters · High-pass / Low-pass</summary>${cutoff(i,'highPass','High-pass',c.highPass)}${cutoff(i,'lowPass','Low-pass',c.lowPass)}</details><details class="fold" data-fold="${i}-eq" ${foldState[i+'-eq']?'open':''}><summary>Parametric EQ · six bands</summary>${c.eq.map((b,j)=>`<div class="band"><strong>Band ${j+1}</strong>${range(i,'frequencyHz','Frequency',b.frequencyHz,20,20000,1,'Hz',j)}${range(i,'gainDb','Gain',b.gainDb,-18,18,.1,'dB',j)}${range(i,'q','Q',b.q,.1,15,.05,'',j)}</div>`).join('')}</details></section>`}
function options(arr,selected){return `<option value="-1">Unassigned</option>`+arr.map((s,j)=>`<option value="${j}" ${j===selected?'selected':''}>${j+1}: ${esc(s)}</option>`).join('')}
async function send(ch,key,value,band){try{await api('/api/set','POST',{channel:ch,key,value,...(band===undefined?{}:{band})})}catch(e){notice(e.message)}}
async function toggleMute(ch){const c=model?.channels?.[ch];if(!c)return;const mute=!Boolean(c.outputMuted);c.outputMuted=mute;const el=document.querySelector(`[data-mute="${ch}"]`);if(el){el.classList.toggle('muted',mute);el.setAttribute('aria-pressed',String(mute));el.textContent=mute?'MUTED':'MUTE OUT'}editingUntil=Date.now()+650;try{await api('/api/set','POST',{channel:ch,key:'outputMuted',value:mute})}catch(e){notice('Mute change failed: '+e.message);await refresh(true)}}
async function clearClip(ch,side){try{await api('/api/set','POST',{channel:ch,key:side==='in'?'clearInputClip':'clearOutputClip',value:true})}catch(e){notice(e.message)}}
function handlers(){document.querySelectorAll('[data-key]').forEach(el=>{const fire=()=>{const ch=Number(el.dataset.ch),key=el.dataset.key,band=el.dataset.band===undefined?undefined:Number(el.dataset.band);let val=el.type==='checkbox'?el.checked:el.type==='range'||(el.tagName==='SELECT'&&key!=='name')?Number(el.value):el.value;if(el.type==='range'&&el.dataset.log==='1')val=Math.round(20*Math.pow(1000,val/1000));editingUntil=Date.now()+1100;if(el.type==='range'){el.nextElementSibling.textContent=String(Number(val).toFixed(Number(el.step)<.1?2:Number(el.step)<1?1:0))+' '+((el.nextElementSibling.textContent.match(/[a-zA-Z/]+$/)||[''])[0]);}if(el.type==='text'){clearTimeout(el._timer);el._timer=setTimeout(()=>send(ch,key,val,band),350)}else send(ch,key,val,band)};el.addEventListener(el.type==='range'?'input':'change',fire)})}
function render(m){
  const previous=model;
  model=m;
  syncGenerator(m.generator);
  $('pin').style.display='none';
  $('app').style.display='block';
  $('startupStatus').textContent=m.loadAtLaunch&&m.startupPresetName?'Load at launch: '+m.startupPresetName+' ✓':m.startupPresetName?'Load at launch: off · selected '+m.startupPresetName:'Load at launch: not configured';
  const choices=m.channels.map((c,i)=>`<option value="${i}">CH ${i+1} · ${esc(c.name)}</option>`).join('');
  if($('channelPicker').innerHTML!==choices)$('channelPicker').innerHTML=choices;
  $('channelPicker').value=String(selectedTab);
  const visibleState=data=>data&&[data.channels[selectedTab],data.inputs,data.outputs];
  if(JSON.stringify(visibleState(previous))!==JSON.stringify(visibleState(m)))renderChannel();
}

function graph(canvas,c,t){if(!canvas)return;const ctx=canvas.getContext('2d'),w=canvas.width,h=canvas.height,pad=36,right=13,top=14,bottom=24;ctx.clearRect(0,0,w,h);ctx.font='20px system-ui';ctx.lineWidth=1;const x=f=>pad+(Math.log10(f/20)/3)*(w-pad-right),y=db=>top+(24-db)/48*(h-top-bottom);ctx.strokeStyle='#284052';ctx.fillStyle='#839ab1';[20,50,100,200,500,1000,2000,5000,10000,20000].forEach(f=>{let a=x(f);ctx.beginPath();ctx.moveTo(a,top);ctx.lineTo(a,h-bottom);ctx.stroke();if([20,100,1000,10000].includes(f))ctx.fillText(f>=1000?(f/1000)+'k':f,a-10,h-5)});[-24,-12,0,12,24].forEach(db=>{let a=y(db);ctx.strokeStyle=db===0?'#62889a':'#284052';ctx.beginPath();ctx.moveTo(pad,a);ctx.lineTo(w-right,a);ctx.stroke();ctx.fillText(String(db),2,a+6)});const yr=db=>top+(0-Math.max(-96,Math.min(0,db)))/96*(h-top-bottom);const drawRta=(bins,stroke,fill,width)=>{if(!Array.isArray(bins)||bins.length<2)return;ctx.beginPath();bins.forEach((db,j)=>{const a=x(20*Math.pow(1000,j/(bins.length-1)));j?ctx.lineTo(a,yr(db)):ctx.moveTo(a,yr(db))});ctx.lineTo(w-right,h-bottom);ctx.lineTo(pad,h-bottom);ctx.closePath();ctx.fillStyle=fill;ctx.fill();ctx.beginPath();bins.forEach((db,j)=>{const a=x(20*Math.pow(1000,j/(bins.length-1)));j?ctx.lineTo(a,yr(db)):ctx.moveTo(a,yr(db))});ctx.strokeStyle=stroke;ctx.lineWidth=width;ctx.stroke()};drawRta(t?.inputRta,'rgba(101,202,216,.84)','rgba(101,202,216,.07)',1.7);drawRta(t?.rta,'rgba(255,105,180,.90)','rgba(255,105,180,.13)',2);if(t&&(Array.isArray(t.rta)||Array.isArray(t.inputRta))){ctx.fillStyle='#b1c8d8';ctx.fillText('0',w-29,top+5);ctx.fillText('-48',w-46,yr(-48)+7);ctx.fillText('-96',w-46,yr(-96)-3);}ctx.strokeStyle='#f6cd70';ctx.lineWidth=4;ctx.beginPath();c.curve.forEach((db,j)=>{let a=x(20*Math.pow(1000,j/(c.curve.length-1)));j?ctx.lineTo(a,y(Math.max(-24,Math.min(24,db)))):ctx.moveTo(a,y(Math.max(-24,Math.min(24,db))))});ctx.stroke();c.eq.forEach((b,j)=>{ctx.beginPath();ctx.arc(x(b.frequencyHz),y(b.gainDb),6,0,7);ctx.fillStyle='#f6cc65';ctx.fill();});}

function meterPercent(db){return Math.max(0,Math.min(100,(Number(db)+72)/72*100))}
function paintMeters(){
  if(telemetry?.channel!==selectedTab)return;
  for(const side of ['in','out']){
    const key=selectedTab+'-'+side,prefix=side==='in'?'input':'output';
    const peak=Number(telemetry[prefix+'Peak']), rms=Number(telemetry[prefix+'Rms']);
    const peakEl=document.querySelector(`[data-peak="${key}"]`),rmsEl=document.querySelector(`[data-rms="${key}"]`),valEl=document.querySelector(`[data-meter-value="${key}"]`);
    if(!peakEl||!rmsEl||!valEl)continue;
    peakEl.style.left=`${meterPercent(peak)}%`;
    rmsEl.style.width=`${meterPercent(rms)}%`;
    valEl.textContent=peak.toFixed(1);
    peakEl.closest('.meterline')?.classList.toggle('clip',Boolean(telemetry[prefix+'Clip']));
  }
}
async function refreshTelemetry(){
  if(telemetryBusy||!model)return;
  telemetryBusy=true;
  const requested=selectedTab;
  try{
    const live=await api('/api/telemetry/'+requested);
    if(live.channel!==selectedTab)return;
    telemetry=live;
    paintMeters();
    graph(document.querySelector('[data-graph]'),model.channels[selectedTab],live);
  }catch(e){/* main state poll reports network/auth failures */}
  finally{telemetryBusy=false}
}

async function refresh(force){if(busy||!force&&(Date.now()<editingUntil||document.activeElement?.matches('input[data-key="name"]')))return;busy=true;try{const state=await api('/api/state');$('status').textContent='Connected · '+state.device;render(state);}catch(e){$('status').textContent=e.message;if(e.message.includes('PIN'))$('pin').style.display='block'}finally{busy=false}}
async function presets(){try{const selected=$('presets').value;const m=await api('/api/presets');$('presets').innerHTML=`<option value="">Select preset</option>`+m.names.map(n=>`<option value="${esc(n)}">${esc(n)}</option>`).join('');$('presets').value=m.names.includes(selected)?selected:''}catch(e){}}
async function savePreset(){const name=$('presetname').value.trim();if(!name){notice('Enter a preset name');return}try{await api('/api/save','POST',{name});notice('Saved on Windows PC');await presets();const clean=name.replace(/[^a-zA-Z0-9 _()\-]/g,'').trim().slice(0,60);$('presets').value=clean}catch(e){notice(e.message)}}
async function loadPreset(){const name=$('presets').value;if(!name){notice('Choose a preset');return}try{await api('/api/load','POST',{name});notice('Preset loaded');setTimeout(()=>refresh(true),200)}catch(e){notice(e.message)}}
async function setStartupPreset(){const name=$('presets').value;if(!name){notice('Choose a saved preset first');return}try{const r=await api('/api/startup','POST',{name});if(!r.ok||!r.loadAtLaunch)throw Error('Startup preset not confirmed');$('startupStatus').textContent='Load at launch: '+r.name+' ✓';notice('Startup preset confirmed on Windows: '+r.name);await refresh(true)}catch(e){notice('Could not set startup preset: '+e.message)}}
$('pinvalue').addEventListener('keydown',e=>{if(e.key==='Enter')connectPin()});refresh(true);setInterval(()=>refresh(false),1100);setInterval(refreshTelemetry,100);setTimeout(refreshTelemetry,150);setInterval(presets,8000);setTimeout(presets,500);
</script></body></html>)DSP_HTML";
