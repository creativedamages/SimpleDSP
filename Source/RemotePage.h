#pragma once

// Single-file mobile controller. All resources are served locally by the Windows app.
static constexpr const char* kRemotePage = R"DSP_HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>SimpleDSP</title>
<meta name="theme-color" content="#111821"><meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-title" content="SimpleDSP">
<link rel="manifest" href="/manifest.webmanifest"><style>
:root{color-scheme:dark;--bg:#111821;--panel:#1d2734;--card:#253140;--muted:#a0b3c7;--accent:#65cad8;--input:#65cad8;--output:#ff69b4;--eq-curve:#f6cd70;--border:#34485d}
*{box-sizing:border-box}body{font:15px system-ui,-apple-system,'Segoe UI',sans-serif;background:var(--bg);color:#edf5fb;margin:0;padding:16px max(12px,env(safe-area-inset-right)) 45px}header{max-width:1160px;margin:auto;display:flex;gap:12px;justify-content:space-between;align-items:center;flex-wrap:wrap}h1{font-size:21px;margin:0}h2{font-size:19px;margin:0 0 14px;color:white}.muted,small{color:var(--muted)}.status{font-size:12px;color:var(--accent)}.bar{max-width:1160px;margin:14px auto;display:flex;flex-wrap:wrap;gap:8px;align-items:center}.bar input{width:145px}.channels{max-width:1160px;margin:auto;display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:15px}.panel{background:var(--panel);border:1px solid var(--border);border-radius:15px;padding:16px;min-width:0}.control{display:grid;grid-template-columns:98px 1fr 65px;gap:7px;align-items:center;margin:9px 0}.control label{font-size:13px;color:var(--muted)}.control output{text-align:right;font-variant-numeric:tabular-nums;font-size:12px;color:#e1eef4}input[type=range]{width:100%;accent-color:var(--accent);min-width:0}input[type=text],select{background:#111c28;color:white;border:1px solid #476073;border-radius:8px;padding:9px;min-width:0;width:100%}button{background:var(--card);color:white;border:1px solid #547081;border-radius:8px;padding:10px;cursor:pointer}button.primary{background:#287d91;border-color:#5bbbcf}button:active{filter:brightness(1.3)}.routing{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:12px}.routing label{font-size:12px;color:var(--muted)}.checkline{display:flex;align-items:center;gap:7px;font-size:13px;margin:12px 0}.checkline input{accent-color:var(--accent)}.filter{background:var(--card);border-radius:9px;padding:10px;margin:11px 0}.filter .head{display:flex;justify-content:space-between;gap:8px;align-items:center}.filter select{width:104px;padding:6px}.band{background:var(--card);border-radius:8px;padding:9px;margin:9px 0}.band strong{font-size:12px;color:var(--accent)}.graph{width:100%;height:clamp(205px,34vw,290px);display:block;background:#101923;border-radius:8px;margin-bottom:13px}.group{display:grid;grid-template-columns:1fr 1fr;gap:10px}.toast{position:fixed;bottom:10px;left:12px;right:12px;max-width:480px;margin:auto;background:#153644;padding:12px;border-radius:9px;text-align:center;display:none}#pin{display:none;max-width:400px;margin:10vh auto;padding:25px;background:var(--panel);border-radius:14px}#pin input{margin:12px 0;font-size:20px;letter-spacing:4px;text-align:center} .startup-status{max-width:1160px;margin:0 auto 15px;color:var(--accent);font-size:13px;min-height:18px}
.channel-title{display:flex;align-items:center;gap:12px;margin-bottom:14px}.channel-title h2{white-space:nowrap;margin:0}.channel-title input{min-width:0;flex:1;font-weight:600;font-size:17px}.channel-title .mute{white-space:nowrap;min-width:93px;padding:10px 8px;border:1px solid #a94f69;color:#ffb5d7;font-weight:700;font-size:12px;background:#293947}.channel-title .mute.muted{background:#ab2348;border-color:#ff719c;color:#fff}
.meters{display:grid;gap:7px;background:#111c28;border-radius:9px;padding:10px 10px 8px;margin-bottom:11px}.meterheading{color:var(--muted);font-size:10px;letter-spacing:.08em}.meterline{display:grid;grid-template-columns:38px minmax(0,1fr) 77px 44px;gap:8px;align-items:center}.meterlabel{font-size:11px;color:var(--muted)}.metertrack{height:13px;position:relative;border-radius:4px;background:#293745;overflow:visible}.meterrms{height:100%;width:0;background:var(--input);border-radius:3px;opacity:.65}.meterline.out .meterrms{background:var(--output)}.meterpeak{position:absolute;top:-2px;left:0%;width:3px;height:17px;border-radius:2px;background:#a5f0fb}.meterline.out .meterpeak{background:#ffa6d1}.meterline.clip .meterpeak{background:#ff5f6f}.clipbtn{height:21px;padding:0 3px;border-radius:4px;border:1px solid #3f5262;background:#293745;color:#8599ab;font-size:10px;font-weight:700}.meterline.clip .clipbtn{background:#db3159;border-color:#ff5b82;color:#fff}.meterline.in .meterlabel{color:var(--input)}.meterline.out .meterlabel{color:var(--output)}.metervalue{font-size:12px;text-align:right;font-variant-numeric:tabular-nums}.fold{margin:9px 0 14px;border:1px solid var(--border);border-radius:9px;overflow:hidden}.fold summary{cursor:pointer;padding:13px 12px;color:var(--accent);font-weight:650;font-size:14px;background:var(--card);list-style-position:inside}.fold[open]{padding-bottom:8px}.fold[open]>summary{border-bottom:1px solid var(--border);margin-bottom:8px}.fold>.filter,.fold>.band{margin:8px 10px}
@media(max-width:790px){.channels{display:block}.panel{margin-bottom:14px}.bar{gap:7px}.bar input{flex:1}.routing{gap:7px}h1{font-size:19px}}
 .channel-select{max-width:840px;margin:12px auto 10px;display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px}
.channel-select button{min-height:44px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;text-align:left}
.channel-select button[aria-pressed="true"]{border-color:var(--accent);background:#1c4656}
.channels,.support{display:block;max-width:840px;margin:0 auto}
.support .bar input,.support .bar select{width:auto;max-width:100%}
.graph-tools{display:flex;flex-wrap:wrap;gap:7px;margin-bottom:12px}
.graph-tools button{font-size:12px;min-height:36px;padding:6px 9px}
.graph-tools button[data-trace][aria-pressed="false"]{color:var(--muted);opacity:.7}
.graph-tools button[aria-pressed="true"]{border-color:currentColor}
.graph-tools [data-trace="eq"]{color:var(--eq-curve)}
.graph-tools [data-trace="input"]{color:var(--input)}
.graph-tools [data-trace="output"]{color:var(--output)}
#sweepActions{max-width:840px;margin:10px auto;font-size:13px}
.channel-title input[type=text]{font-size:16px}
@media(max-width:480px){.panel{padding:12px}.clipbtn{height:30px}.control{grid-template-columns:84px minmax(0,1fr) 58px}.routing select{font-size:16px}.support input{font-size:16px}}
</style></head><body>
<header><div><h1>SimpleDSP</h1></div><span id="status" class="status">Connecting…</span></header>
<div id="pin"><h2>Enter remote PIN</h2><p class="muted">Enter the six-digit PIN shown on the Windows computer.</p><input type="text" id="pinvalue" inputmode="numeric" maxlength="6" placeholder="000000"><button class="primary" onclick="connectPin()">Connect</button></div>
<div id="app" style="display:none">
<div class="channel-select" id="channelPicker" role="group" aria-label="Channel"></div>
<div id="sweepActions" hidden><button id="stopSweep" onclick="stopSineSweep()" disabled>Stop sweep</button> <span id="sweepStatus" role="status"></span></div>
<main class="channels" id="channels"></main>
<div class="support"><details class="fold"><summary>Presets</summary><div class="bar"><input id="presetname" type="text" placeholder="Preset name" maxlength="60"><button onclick="savePreset()">Save preset</button><select id="presets" aria-label="Saved presets"><option>Saved presets</option></select><button onclick="loadPreset()">Load preset</button><button class="primary" onclick="setStartupPreset()">Set as load-at-launch</button></div><div id="startupStatus" class="startup-status" role="status">Startup preset: not configured</div></details>
<details class="fold"><summary>Test generator</summary><section class="panel">
<label class="checkline"><input id="generatorEnabled" type="checkbox" onchange="setGenerator('enabled',this.checked)">Test generator ON</label>
<select id="generatorMode" aria-label="Test signal" onchange="setGenerator('mode',Number(this.value))"><option value="0">Pink noise</option><option value="1">Sine oscillator</option><option value="2">1 kHz tone</option></select>
<div class="control"><label for="generatorLevel">Global level</label><input id="generatorLevel" type="range" min="-90" max="0" step="0.1" value="-40" oninput="this.nextElementSibling.textContent=this.value+' dBFS';setGenerator('levelDb',Number(this.value))"><output>-40 dBFS</output></div>
<div class="control"><label for="generatorFrequency">Frequency</label><input id="generatorFrequency" type="number" min="20" max="20000" step="1" value="1000" onchange="setGenerator('frequency',Number(this.value))"><output>Hz</output></div>
</section></details></div></div><div class="toast" id="toast"></div>
<script>
)DSP_HTML"
R"DSP_HTML(let pin=localStorage.getItem('htDspPin')||'',model=null,telemetry=null,selectedTab=0,busy=false,editingUntil=0,telemetryBusy=false;
const foldState={}, traces={eq:true,input:true,output:true};
let frozenRta=null;
const $=id=>document.getElementById(id),esc=s=>String(s??'').replace(/[&<>"']/g,x=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[x]));
function notice(s){$('toast').textContent=s;$('toast').style.display='block';setTimeout(()=>$('toast').style.display='none',2800)}
async function api(path,method='GET',data){const r=await fetch(path,{method,headers:{'X-DSP-PIN':pin,'Content-Type':'application/json'},body:data?JSON.stringify(data):undefined,cache:'no-store'});if(r.status===401){$('app').style.display='none';$('pin').style.display='block';throw Error('Enter PIN from the Windows application')}if(!r.ok)throw Error((await r.text()).slice(0,120));return r.json()}
function connectPin(){pin=$('pinvalue').value.trim();localStorage.setItem('htDspPin',pin);refresh(true)}
async function playSweep(channel){
  try{
    const current=await api('/api/state');
    if(current.generator.sweepChannel>=0){notice('A sweep is already running.');return}
    if(current.channels[channel].outputRoute<0){notice('Assign an output route first.');return}
    if(!confirm('Proceed with caution. Play a 10-second 20 Hz to 20 kHz sine sweep on CH '+(channel+1)+' at '+current.generator.levelDb+' dBFS before channel processing? This temporarily unmutes this channel and mutes all other outputs. Previous mute states return afterward.'))return;
    await api('/api/set','POST',{channel,key:'sweep',confirmed:true});
    setTimeout(()=>refresh(true),150);
  }catch(e){notice(e.message)}
}
async function stopSineSweep(){
  try{await api('/api/set','POST',{key:'stopSweep'});setTimeout(()=>refresh(true),150)}
  catch(e){notice(e.message)}
}
function syncTestMute(){
  if(!model)return;
  const ch=selectedTab,c=model.channels[ch],g=model.generator;
  const mutedByTest=g?.enabled && !c.testInput && g.sweepChannel<0;
  const button=document.querySelector('[data-mute]');
  if(!button)return;
  button.disabled=Boolean(mutedByTest);
  button.classList.toggle('muted',Boolean(mutedByTest||c.outputMuted));
  button.setAttribute('aria-pressed',String(Boolean(mutedByTest||c.outputMuted)));
  button.textContent=mutedByTest?'TEST MUTED':c.outputMuted?'MUTED':'MUTE OUT';
}
function syncSweep(g){
  const channel=g?.sweepChannel??-1;
  $('stopSweep').disabled=channel<0;
  $('sweepActions').hidden=channel<0;
  $('sweepStatus').textContent=channel>=0?' Sweeping CH '+(channel+1)+' - all other outputs muted':'';
  document.querySelectorAll('[data-sweep]').forEach(button=>{
    button.disabled=channel>=0;
    button.textContent=Number(button.dataset.sweep)===channel?'Sweeping...':'Sine sweep...';
  });
}
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
  if(!model?.channels[index]||index===selectedTab)return;
  selectedTab=index;
  frozenRta=null;
  syncChannelButtons();
  renderChannel();
  refreshTelemetry();
}
function renderChannel(){
  document.querySelectorAll('details[data-fold]').forEach(el=>{foldState[el.dataset.fold]=el.open});
  const c=model.channels[selectedTab];
  $('channels').innerHTML=channel(c,selectedTab,model);
  handlers();
  syncGraphControls();
  paintGraph();
  paintMeters();
  syncSweep(model.generator);
  syncTestMute();
}

const format=(v,d=1)=>Number(v).toFixed(d);function range(ch,key,label,val,min,max,step,unit,band){const idx=band===undefined?'':` data-band="${band}"`,log=key==='frequencyHz'||key.endsWith('.frequency');return `<div class="control"><label>${label}</label><input type="range" data-ch="${ch}" data-key="${key}"${idx} data-log="${log?'1':'0'}" min="${log?0:min}" max="${log?1000:max}" step="${log?1:step}" value="${log?Math.round(Math.log(Math.max(20,val)/20)/Math.log(1000)*1000):val}"><output>${format(val,step===.25||step<.1?2:step<1?1:0)} ${unit}</output></div>`}
function cutoff(ch,key,caption,filter){return `<div class="filter"><div class="head"><label class="checkline"><input type="checkbox" data-ch="${ch}" data-key="${key}.enabled" ${filter.enabled?'checked':''}>${caption}</label><select data-ch="${ch}" data-key="${key}.slope"><option value="12" ${filter.slope==12?'selected':''}>12 dB/oct</option><option value="24" ${filter.slope==24?'selected':''}>24 dB/oct</option></select></div>${range(ch,key+'.frequency','Frequency',filter.frequency,20,20000,1,'Hz')}</div>`}
function meterHtml(i){return `<div class="meters"><div class="meterheading">INPUT SAMPLE PEAK · OUTPUT TRUE PEAK</div><div class="meterline in"><span class="meterlabel">IN</span><div class="metertrack"><div class="meterrms" data-rms="${i}-in"></div><div class="meterpeak" data-peak="${i}-in"></div></div><span class="metervalue" data-meter-value="${i}-in">-96.0 dBFS</span><button type="button" class="clipbtn" onclick="clearClip(${i},'in')" aria-label="Clear input clip indicator">CLIP</button></div><div class="meterline out"><span class="meterlabel">OUT</span><div class="metertrack"><div class="meterrms" data-rms="${i}-out"></div><div class="meterpeak" data-peak="${i}-out"></div></div><span class="metervalue" data-meter-value="${i}-out">-96.0 dBTP</span><button type="button" class="clipbtn" onclick="clearClip(${i},'out')" aria-label="Clear output true-peak overload">CLIP</button></div></div>`}
function channel(c,i,m){
  return `<section class="panel" data-channel="${i}">
  ${meterHtml(i)}
  <canvas class="graph" data-graph="${i}" role="img" aria-label="Frequency response and real-time spectrum"></canvas>
  <div class="graph-tools" role="group" aria-label="Spectrum controls">
    <button type="button" data-trace="eq" onclick="toggleTrace('eq')">EQ</button>
    <button type="button" data-trace="input" onclick="toggleTrace('input')">Input</button>
    <button type="button" data-trace="output" onclick="toggleTrace('output')">Output</button>
    <button type="button" id="freezeRta" onclick="freezeSpectrum()">Freeze RTA</button>
  </div>
  <div class="channel-title"><h2>CH ${i+1}</h2><input type="text" maxlength="40" data-ch="${i}" data-key="name" aria-label="CH ${i+1} name" value="${esc(c.name)}"><button type="button" class="mute ${c.outputMuted?'muted':''}" data-mute="${i}" aria-pressed="${c.outputMuted?'true':'false'}" onclick="toggleMute(${i})">${c.outputMuted?'MUTED':'MUTE OUT'}</button></div>
  <details class="fold" data-fold="${i}-routes" ${foldState[i+'-routes']?'open':''}><summary>Routing and test input</summary>
    <div class="routing">
      <label>Input<select data-ch="${i}" data-key="inputRoute">${options(m.inputs,c.inputRoute)}</select></label>
      <label>Output<select data-ch="${i}" data-key="outputRoute">${options(m.outputs,c.outputRoute)}</select></label>
    </div>
    <label class="checkline"><input type="checkbox" data-test-input="${i}" ${c.testInput?'checked':''} onchange="engageTestInput(${i},this)">Use test signal</label>
    <button type="button" data-sweep="${i}" onclick="playSweep(${i})">Sine sweep...</button>
  </details>
  <details class="fold" data-fold="${i}-level" ${foldState[i+'-level']?'open':''}><summary>Level and delay</summary>${range(i,'levelDb','Level',c.levelDb,-90,18,.1,'dB')}${range(i,'delayMs','Delay',c.delayMs,0,50,.25,'ms')}</details>
  <label class="checkline"><input type="checkbox" data-ch="${i}" data-key="invertPolarity" ${c.invertPolarity?'checked':''}>Invert polarity (180 deg)</label>
  <details class="fold" data-fold="${i}-filters" ${foldState[i+'-filters']?'open':''}><summary>High-pass and low-pass</summary>${cutoff(i,'highPass','High-pass',c.highPass)}${cutoff(i,'lowPass','Low-pass',c.lowPass)}</details>
  <details class="fold" data-fold="${i}-eq" ${foldState[i+'-eq']?'open':''}><summary>Parametric EQ</summary>${c.eq.map((b,j)=>`<div class="band"><strong>Band ${j+1}</strong>${range(i,'frequencyHz','Frequency',b.frequencyHz,20,20000,1,'Hz',j)}${range(i,'gainDb','Gain',b.gainDb,-18,18,.1,'dB',j)}${range(i,'q','Q',b.q,.1,15,.05,'',j)}</div>`).join('')}</details>
  </section>`;
}
function options(arr,selected){return `<option value="-1">Unassigned</option>`+arr.map((s,j)=>`<option value="${j}" ${j===selected?'selected':''}>${j+1}: ${esc(s)}</option>`).join('')}
async function send(ch,key,value,band){try{await api('/api/set','POST',{channel:ch,key,value,...(band===undefined?{}:{band})})}catch(e){notice(e.message)}}
async function toggleMute(ch){const c=model?.channels?.[ch];if(!c)return;const mute=!Boolean(c.outputMuted);c.outputMuted=mute;const el=document.querySelector(`[data-mute="${ch}"]`);if(el){el.classList.toggle('muted',mute);el.setAttribute('aria-pressed',String(mute));el.textContent=mute?'MUTED':'MUTE OUT'}editingUntil=Date.now()+650;try{await api('/api/set','POST',{channel:ch,key:'outputMuted',value:mute})}catch(e){notice('Mute change failed: '+e.message);await refresh(true)}}
async function clearClip(ch,side){try{await api('/api/set','POST',{channel:ch,key:side==='in'?'clearInputClip':'clearOutputClip',value:true})}catch(e){notice(e.message)}}
function handlers(){document.querySelectorAll('[data-key]').forEach(el=>{const fire=()=>{const ch=Number(el.dataset.ch),key=el.dataset.key,band=el.dataset.band===undefined?undefined:Number(el.dataset.band);let val=el.type==='checkbox'?el.checked:el.type==='range'||(el.tagName==='SELECT'&&key!=='name')?Number(el.value):el.value;if(el.type==='range'&&el.dataset.log==='1')val=Math.round(20*Math.pow(1000,val/1000));editingUntil=Date.now()+1100;if(el.type==='range'){el.nextElementSibling.textContent=String(Number(val).toFixed(el.dataset.key==='delayMs'||Number(el.step)<.1?2:Number(el.step)<1?1:0))+' '+((el.nextElementSibling.textContent.match(/[a-zA-Z/]+$/)||[''])[0]);}if(el.type==='text'){clearTimeout(el._timer);el._timer=setTimeout(()=>send(ch,key,val,band),350)}else send(ch,key,val,band)};el.addEventListener(el.type==='range'?'input':'change',fire)})}
function render(m){
  const previous=model;
  model=m;
  syncGenerator(m.generator);
  syncSweep(m.generator);
  syncTestMute();
  $('pin').style.display='none';
  $('app').style.display='block';
  $('startupStatus').textContent=m.loadAtLaunch&&m.startupPresetName?'Load at launch: '+m.startupPresetName+' ✓':m.startupPresetName?'Load at launch: off · selected '+m.startupPresetName:'Load at launch: not configured';
  const labels=m.channels.map((c,i)=>'CH '+(i+1)+' · '+c.name);
  if(JSON.stringify(labels)!==JSON.stringify(previous?.channels.map((c,i)=>'CH '+(i+1)+' · '+c.name)))
    $('channelPicker').innerHTML=labels.map((label,i)=>`<button type="button" data-channel-button="${i}" onclick="selectChannel(${i})">${esc(label)}</button>`).join('');
  syncChannelButtons();
  const visibleState=data=>data&&[data.channels[selectedTab],data.inputs,data.outputs,data.rtaMaxHz];
  if(JSON.stringify(visibleState(previous))!==JSON.stringify(visibleState(m)))renderChannel();
}

)DSP_HTML"
R"DSP_HTML(function syncChannelButtons(){
  document.querySelectorAll('[data-channel-button]').forEach(button=>
    button.setAttribute('aria-pressed',String(Number(button.dataset.channelButton)===selectedTab)));
}
function syncGraphControls(){
  document.querySelectorAll('[data-trace]').forEach(button=>
    button.setAttribute('aria-pressed',String(traces[button.dataset.trace])));
  const freeze=$('freezeRta');
  if(!freeze)return;
  freeze.disabled=telemetry?.channel!==selectedTab&&!frozenRta;
  freeze.setAttribute('aria-pressed',String(Boolean(frozenRta)));
  freeze.textContent=frozenRta?'Resume RTA':'Freeze RTA';
}
function toggleTrace(trace){
  traces[trace]=!traces[trace];
  syncGraphControls();
  paintGraph();
}
function freezeSpectrum(){
  if(frozenRta)frozenRta=null;
  else if(telemetry?.channel===selectedTab)frozenRta={data:telemetry,maxHz:model.rtaMaxHz};
  syncGraphControls();
  paintGraph();
}
function paintGraph(){
  const canvas=document.querySelector('[data-graph]');
  if(!canvas||!canvas.clientWidth||!model)return;
  const c=model.channels[selectedTab], t=frozenRta?.data||(telemetry?.channel===selectedTab?telemetry:null);
  const maxHz=frozenRta?.maxHz||model.rtaMaxHz||20000;
  const w=canvas.clientWidth,h=canvas.clientHeight,dpr=Math.min(window.devicePixelRatio||1,3);
  const width=Math.round(w*dpr),height=Math.round(h*dpr);
  if(canvas.width!==width||canvas.height!==height){canvas.width=width;canvas.height=height}
  const ctx=canvas.getContext('2d'), pad=37,right=38,top=30,bottom=h-24;
  ctx.setTransform(dpr,0,0,dpr,0,0);
  ctx.clearRect(0,0,w,h);
  ctx.font='11px system-ui';
  const x=f=>pad+(Math.log10(f/20)/3)*(w-pad-right);
  const y=db=>top+(24-Math.max(-24,Math.min(24,db)))/48*(bottom-top);
  const yr=db=>top+(0-Math.max(-96,Math.min(0,db)))/96*(bottom-top);
  const line=(x1,y1,x2,y2,color)=>{
    ctx.strokeStyle=color;ctx.lineWidth=1;ctx.beginPath();ctx.moveTo(x1,y1);ctx.lineTo(x2,y2);ctx.stroke();
  };
  ctx.fillStyle='#b2c6d8';
  ctx.textAlign='left';
  if(traces.eq)ctx.fillText('EQ dB',8,16);
  ctx.textAlign='right';
  if(traces.input||traces.output)ctx.fillText('RTA dBFS',w-8,16);
  for(const db of [-24,-12,0,12,24]){
    const a=y(db);
    line(pad,a,w-right,a,db===0?'#59758a':'#284052');
    if(traces.eq)ctx.fillText(String(db),pad-6,a+4);
  }
  if(traces.input||traces.output){
    ctx.textAlign='left';
    for(const db of [0,-24,-48,-72,-96])ctx.fillText(String(db),w-right+6,yr(db)+4);
  }
  ctx.textAlign='center';
  for(const hz of [20,50,100,200,500,1000,2000,5000,10000,20000]){
    const a=x(hz);
    line(a,top,a,bottom,'#284052');
    if([20,100,1000,10000,20000].includes(hz))
      ctx.fillText(hz>=1000?(hz/1000)+'k':String(hz),a,h-6);
  }
  const trace=(bins,limit,yScale,stroke,width)=>{
    if(!Array.isArray(bins)||bins.length<2)return;
    ctx.beginPath();
    bins.forEach((db,j)=>{
      const a=x(20*Math.pow(limit/20,j/(bins.length-1)));
      if(j)ctx.lineTo(a,yScale(db));else ctx.moveTo(a,yScale(db));
    });
    ctx.strokeStyle=stroke;ctx.lineWidth=width;ctx.stroke();
  };
  if(traces.input)trace(t?.inputRta,maxHz,yr,'#65cad8',1.2);
  if(traces.output)trace(t?.rta,maxHz,yr,'#ff69b4',1.5);
  if(traces.eq){
    trace(c.curve,20000,y,'#f6cd70',2);
    ctx.fillStyle='#f6cd70';
    for(const band of c.eq){
      ctx.beginPath();ctx.arc(x(band.frequencyHz),y(band.gainDb),3,0,2*Math.PI);ctx.fill();
    }
  }
}
window.addEventListener('resize',paintGraph);

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
    valEl.textContent=peak.toFixed(1)+(side==='out'?' dBTP':' dBFS');
    peakEl.closest('.meterline')?.classList.toggle('clip',Boolean(telemetry[prefix+'Clip']));
  }
}
async function refreshTelemetry(){
  if(telemetryBusy||!model||document.hidden||$('app').style.display==='none')return;
  telemetryBusy=true;
  const requested=selectedTab;
  try{
    const live=await api('/api/telemetry/'+requested);
    if(live.channel!==selectedTab)return;
    telemetry=live;
    paintMeters();
    syncGraphControls();
    if(!frozenRta)paintGraph();
  }catch(e){/* main state poll reports network/auth failures */}
  finally{telemetryBusy=false}
}

)DSP_HTML"
R"DSP_HTML(async function refresh(force){if(busy||!force&&(document.hidden||Date.now()<editingUntil||document.activeElement?.matches('input[data-key="name"]')))return;busy=true;try{const state=await api('/api/state');$('status').textContent='Connected · '+state.device;render(state);}catch(e){$('status').textContent=e.message;if(e.message.includes('PIN'))$('pin').style.display='block'}finally{busy=false}}
async function presets(){if(document.hidden)return;try{const selected=$('presets').value;const m=await api('/api/presets');$('presets').innerHTML=`<option value="">Select preset</option>`+m.names.map(n=>`<option value="${esc(n)}">${esc(n)}</option>`).join('');$('presets').value=m.names.includes(selected)?selected:''}catch(e){}}
async function savePreset(){const name=$('presetname').value.trim();if(!name){notice('Enter a preset name');return}try{await api('/api/save','POST',{name});notice('Saved on Windows PC');await presets();const clean=name.replace(/[^a-zA-Z0-9 _()\-]/g,'').trim().slice(0,60);$('presets').value=clean}catch(e){notice(e.message)}}
async function loadPreset(){const name=$('presets').value;if(!name){notice('Choose a preset');return}try{await api('/api/load','POST',{name});notice('Preset loaded');setTimeout(()=>refresh(true),200)}catch(e){notice(e.message)}}
async function setStartupPreset(){const name=$('presets').value;if(!name){notice('Choose a saved preset first');return}try{const r=await api('/api/startup','POST',{name});if(!r.ok||!r.loadAtLaunch)throw Error('Startup preset not confirmed');$('startupStatus').textContent='Load at launch: '+r.name+' ✓';notice('Startup preset confirmed on Windows: '+r.name);await refresh(true)}catch(e){notice('Could not set startup preset: '+e.message)}}
$('pinvalue').addEventListener('keydown',e=>{if(e.key==='Enter')connectPin()});document.addEventListener('visibilitychange',()=>{if(!document.hidden){refresh(true);refreshTelemetry()}});refresh(true);setInterval(()=>refresh(false),1100);setInterval(refreshTelemetry,100);setTimeout(refreshTelemetry,150);setInterval(presets,8000);setTimeout(presets,500);
</script></body></html>)DSP_HTML";
