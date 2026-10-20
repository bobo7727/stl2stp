/* STL2STP clickable prototype. All numbers shown are EXAMPLE values computed from a
   built-in demo mesh or the documented desktop benchmark; nothing comes from a real conversion. */
(function(){
'use strict';
const Q = new URLSearchParams(location.search);
const root = document.documentElement;
let LANG = Q.get('lang') === 'en' ? 'en' : 'zh';
function applyTheme(th){
  const t = th || Q.get('theme') || 'system';
  const dark = t === 'dark' || (t === 'system' && matchMedia('(prefers-color-scheme: dark)').matches);
  root.dataset.theme = dark ? 'dark' : 'light';
}
applyTheme();
if (Q.get('text') === 'xl') root.dataset.text = 'xl';
if (Q.get('bare') === '1') document.body.classList.add('bare');
const IPAD = Q.get('device') === 'ipad';

function t(k, v){ let s = (STRINGS[LANG] && STRINGS[LANG][k]) || STRINGS.zh[k] || k;
  if (v) for (const n in v) s = s.split('{'+n+'}').join(v[n]); return s; }
const esc = s => String(s).replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const fmtInt = n => Math.round(n).toLocaleString(LANG==='zh'?'zh-CN':'en-US');
function fmtBytes(b){ if (b < 1024*1024) return Math.max(1,Math.round(b/1024)) + ' KB';
  if (b < 1024**3) return (b/1024/1024).toFixed(b<10*1024*1024?1:0) + ' MB'; return (b/1024**3).toFixed(1) + ' GB'; }
function fmtTime(s){ if (s < 1) return t('estimate.under_1s'); if (s < 90) return t('estimate.seconds',{n:Math.round(s)});
  return t('estimate.minutes',{n:Math.round(s/60)}); }

/* ---------------- icons (SF Symbols names noted in ui-spec.md) ---------------- */
const P = {
  doc:'<path d="M7 3h7l5 5v13H7z"/><path d="M14 3v5h5"/>',
  import:'<path d="M12 4v11"/><path d="m7.5 10.5 4.5 4.5 4.5-4.5"/><path d="M5 19h14"/>',
  cube:'<path d="m12 3 8 4.5v9L12 21l-8-4.5v-9z"/><path d="m4 7.5 8 4.5 8-4.5M12 12v9"/>',
  share:'<path d="M12 3v12"/><path d="m8 7 4-4 4 4"/><path d="M6 11H5v10h14V11h-1"/>',
  folder:'<path d="M3 6h6l2 2h10v11H3z"/>',
  gear:'<circle cx="12" cy="12" r="3.2"/><path d="M12 2.5v3M12 18.5v3M2.5 12h3M18.5 12h3M5.3 5.3l2.1 2.1M16.6 16.6l2.1 2.1M5.3 18.7l2.1-2.1M16.6 7.4l2.1-2.1"/>',
  clock:'<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/>',
  check:'<path d="m5 12.5 4.5 4.5L19 7.5"/>',
  x:'<path d="M6 6l12 12M18 6 6 18"/>',
  warn:'<path d="M12 3 2 20h20z"/><path d="M12 10v4.5M12 17.2v.3"/>',
  info:'<circle cx="12" cy="12" r="9"/><path d="M12 11v6M12 7.5v.3"/>',
  chevR:'<path d="m9 5 7 7-7 7"/>', chevL:'<path d="m15 5-7 7 7 7"/>',
  reset:'<path d="M4 12a8 8 0 1 0 2.4-5.7"/><path d="M4 4v4.5h4.5"/>',
  grid:'<path d="M4 4h16v16H4zM4 12h16M12 4v16M4 4l16 16"/>',
  bolt:'<path d="M13 2 5 13h6l-1 9 8-11h-6z"/>',
  shrink:'<path d="M4 9h5V4M20 9h-5V4M4 15h5v5M20 15h-5v5"/>',
  shield:'<path d="M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6z"/><path d="m8.5 12 2.5 2.5 4.5-5"/>',
  lock:'<rect x="5" y="10" width="14" height="10" rx="2"/><path d="M8 10V7a4 4 0 0 1 8 0v3"/>',
  trash:'<path d="M4 7h16M9 7V4h6v3M6 7l1 13h10l1-13"/>',
  stop:'<rect x="6" y="6" width="12" height="12" rx="2"/>',
  memory:'<rect x="4" y="7" width="16" height="10" rx="1.5"/><path d="M8 7v10M12 7v10M16 7v10M4 20h16"/>',
  big:'<path d="M4 9V4h5M20 9V4h-5M4 15v5h5M20 15v5h-5"/><path d="M9 9h6v6H9z"/>',
  chat:'<path d="M4 5h16v11H9l-5 4z"/>',
  star:'<path d="m12 3 2.7 5.6 6.1.9-4.4 4.3 1 6.1L12 17l-5.4 2.9 1-6.1L3.2 9.5l6.1-.9z"/>',
  text:'<path d="M5 6h14M12 6v13M8.5 19h7"/>',
  globe:'<circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3c3 3.2 3 14.8 0 18M12 3c-3 3.2-3 14.8 0 18"/>',
  moon:'<path d="M20 14.5A8 8 0 0 1 9.5 4a8 8 0 1 0 10.5 10.5z"/>',
  hand:'<path d="M8 13V6.5a1.5 1.5 0 0 1 3 0V12M11 11V5a1.5 1.5 0 0 1 3 0v6M14 11V6.5a1.5 1.5 0 0 1 3 0V14c0 4-2.5 7-6 7-2.5 0-4-1.3-5.5-3.5L3.5 14a1.5 1.5 0 0 1 2.4-1.7L8 14.5"/>',
  mail:'<rect x="3" y="5" width="18" height="14" rx="2"/><path d="m3 7 9 6 9-6"/>',
  airdrop:'<circle cx="12" cy="12" r="2"/><path d="M8.5 15.5a5 5 0 1 1 7 0M5.6 18.4a9 9 0 1 1 12.8 0"/>',
  more:'<circle cx="6" cy="12" r="1.3"/><circle cx="12" cy="12" r="1.3"/><circle cx="18" cy="12" r="1.3"/>',
};
const ic = (n, sw) => `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="${sw||2}" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${P[n]}</svg>`;

/* ---------------- demo meshes ---------------- */
function quad(T,a,b,c,d){ T.push([a,b,c],[a,c,d]); }
function box(T,x0,y0,z0,x1,y1,z1){
  const v=(x,y,z)=>[x,y,z]; const p=[v(x0,y0,z0),v(x1,y0,z0),v(x1,y1,z0),v(x0,y1,z0),v(x0,y0,z1),v(x1,y0,z1),v(x1,y1,z1),v(x0,y1,z1)];
  quad(T,p[0],p[3],p[2],p[1]); quad(T,p[4],p[5],p[6],p[7]); quad(T,p[0],p[1],p[5],p[4]);
  quad(T,p[1],p[2],p[6],p[5]); quad(T,p[2],p[3],p[7],p[6]); quad(T,p[3],p[0],p[4],p[7]); }
function cyl(T,cx,cy,z0,z1,r,n){ for(let i=0;i<n;i++){ const a=i/n*2*Math.PI,b=(i+1)/n*2*Math.PI;
  const A=[cx+r*Math.cos(a),cy+r*Math.sin(a)],B=[cx+r*Math.cos(b),cy+r*Math.sin(b)];
  quad(T,[A[0],A[1],z0],[B[0],B[1],z0],[B[0],B[1],z1],[A[0],A[1],z1]);
  T.push([[cx,cy,z1],[A[0],A[1],z1],[B[0],B[1],z1]]); } }
function prism(T,pts,y0,y1){ const [a,b,c]=pts; const A0=[a[0],y0,a[1]],B0=[b[0],y0,b[1]],C0=[c[0],y0,c[1]],A1=[a[0],y1,a[1]],B1=[b[0],y1,b[1]],C1=[c[0],y1,c[1]];
  T.push([A0,B0,C0],[A1,C1,B1]); quad(T,A0,A1,B1,B0); quad(T,B0,B1,C1,C0); quad(T,C0,C1,A1,A0); }
function sphere(T,r,nu,nv){ const p=(u,v)=>{const th=u/nu*2*Math.PI, ph=v/nv*Math.PI; return [r*Math.sin(ph)*Math.cos(th), r*Math.sin(ph)*Math.sin(th), r*Math.cos(ph)];};
  for(let v=0;v<nv;v++) for(let u=0;u<nu;u++){ const a=p(u,v),b=p(u+1,v),c=p(u+1,v+1),d=p(u,v+1); if(v>0) T.push([a,d,b]); if(v<nv-1) T.push([b,d,c]); } }
function bracket(){ const T=[]; box(T,0,0,0,60,40,8); box(T,0,0,8,8,40,50); prism(T,[[8,8],[30,8],[8,32]],16,24); cyl(T,42,20,8,16,7,28); return T; }
function stats(T){ const seen=new Set(); const mn=[1e9,1e9,1e9],mx=[-1e9,-1e9,-1e9];
  for(const tr of T) for(const p of tr){ seen.add(p.map(x=>x.toFixed(4)).join(',')); for(let i=0;i<3;i++){mn[i]=Math.min(mn[i],p[i]);mx[i]=Math.max(mx[i],p[i]);} }
  return {faces:T.length, vertices:seen.size, size:mx.map((x,i)=>x-mn[i]), bytes:84+50*T.length, binary:true}; }
const FILES = {
  bracket:{name:'示例_支架.stl', nameEn:'example_bracket.stl', mesh:bracket()},
  large:{name:'示例_大模型.stl', nameEn:'example_large.stl', mesh:null},
};
FILES.bracket.st = stats(FILES.bracket.mesh);
{ const T=[]; sphere(T,60,56,36); FILES.large.mesh=T;
  /* large example: documented as an illustrative 600k-face file */
  FILES.large.st={faces:600000, vertices:300002, size:[120,120,120], bytes:84+50*600000, binary:true}; }
const fname = f => LANG==='en' ? f.nameEn : f.name;

/* ---------------- state ---------------- */
const PRESETS = {
  recommended:{simplify:false, ratio:50, mode:'original', merge:false, verify:true},
  smaller:{simplify:true, ratio:50, mode:'original', merge:true, verify:true},
  strict:{simplify:false, ratio:50, mode:'sewing', merge:false, verify:true},
};
const S = Object.assign({route:'home', file:'bracket', fmt:'step', preset:'recommended', advOpen:false,
  meta:{author:'',org:'',product:'',description:''}, wire:false, empty:false, sheet:null}, PRESETS.recommended);
function setPreset(p){ S.preset=p; Object.assign(S, PRESETS[p]); }
function markCustom(){ for (const p in PRESETS){ const d=PRESETS[p]; if (d.simplify===S.simplify && d.mode===S.mode && d.merge===S.merge && d.verify===S.verify && (!d.simplify || d.ratio===S.ratio)) { S.preset=p; return; } } S.preset='custom'; }
function effFaces(){ const f=FILES[S.file].st.faces; return S.simplify ? Math.max(4, Math.round(f*S.ratio/100)) : f; }
/* Estimates: desktop benchmark 19,800 faces -> ~4 s, ~48 MB STEP (~2.4 KB/face, ~0.2 ms/face).
   Phone factor 1.5 and IGES factor are PLACEHOLDERS to be calibrated on device. */
function estimate(){ const n=effFaces(); let sec=n*0.0002*1.5*(S.mode==='sewing'?2:1); if (S.verify && S.fmt==='step') sec*=1.3;
  return {sec, bytes:n*2400, n}; }
const LARGE = 200000, LIMIT = 2000000; // placeholders, calibrate per device RAM

/* ---------------- routing ---------------- */
function parseHash(){ const h=location.hash.replace(/^#\/?/,''); const [r,qs]=h.split('?'); return {r:r||'home', p:new URLSearchParams(qs||'')}; }
function go(r, extra){ location.hash = '#/'+r+(extra?'?'+extra:''); }
window.addEventListener('hashchange', ()=>route());
function route(){ const {r,p}=parseHash(); S.route=r; S.sheet=p.get('sheet'); S.params=p;
  if (p.has('file')) S.file=p.get('file'); if (p.has('fmt')) S.fmt=p.get('fmt');
  if (p.has('preset')) setPreset(p.get('preset')); if (p.has('adv')) S.advOpen=p.get('adv')==='1';
  if (p.has('empty')) S.empty=p.get('empty')==='1';
  if (p.get('meta')==='zh') S.meta={author:'张工',org:'示例公司',product:'Bracket-01',description:''};
  if (p.has('simplify')) { S.simplify=true; S.ratio=+p.get('simplify'); markCustom(); }
  render(); if (p.has('scroll')) { const c=document.querySelector('.device .content'); if (c) c.scrollTop=+p.get('scroll'); } }

/* ---------------- chrome ---------------- */
const status = () => `<div class="status"><span class="num">9:41</span><span class="island"></span><span class="icons">
  <svg width="18" height="12" viewBox="0 0 18 12"><rect x="0" y="8" width="3" height="4" rx="1" fill="currentColor"/><rect x="5" y="5.5" width="3" height="6.5" rx="1" fill="currentColor"/><rect x="10" y="3" width="3" height="9" rx="1" fill="currentColor"/><rect x="15" y="0" width="3" height="12" rx="1" fill="currentColor"/></svg>
  <svg width="27" height="13" viewBox="0 0 27 13"><rect x=".5" y=".5" width="23" height="12" rx="3.5" fill="none" stroke="currentColor" opacity=".4"/><rect x="2" y="2" width="18" height="9" rx="2" fill="currentColor"/><rect x="25" y="4.5" width="1.5" height="4" rx=".7" fill="currentColor" opacity=".5"/></svg></span></div>`;
const homebar = () => IPAD ? '' : '<div class="homebar"></div>';
function nav(title, left, right){ return `<div class="nav">${left||'<span></span>'}<div class="title">${title||''}</div>${right||'<span></span>'}</div>`; }
const backBtn = (to, label) => `<button class="navbtn" data-go="${to}" aria-label="${t('common.back')}">${ic('chevL',2.4)}<span>${label||t('common.back')}</span></button>`;
const toggle = (on, act, dis) => `<div class="toggle ${on?'on':''} ${dis?'dis':''}" role="switch" aria-checked="${on}" tabindex="0" data-act="${act}"></div>`;

/* ---------------- screens ---------------- */
const V = {};
V.onboarding1 = () => `<div class="screen">${status()}
 <div style="display:flex;justify-content:flex-end;padding:0 12px"><button class="navbtn" data-go="home">${t('common.skip')}</button></div>
 <div class="ob"><div class="art">${artMeshToSolid()}</div>
  <div class="t-large" style="text-align:center">${t('onboarding.1.title')}</div>
  <p class="t-body c2" style="text-align:center;margin:12px 0 0">${t('onboarding.1.body')}</p>
  <div class="dots"><i class="on"></i><i></i></div>
  <div style="padding-bottom:44px"><button class="btn btn-primary" data-go="onboarding2">${t('common.next')}</button></div></div>${homebar()}</div>`;
V.onboarding2 = () => `<div class="screen">${status()}
 <div style="display:flex;justify-content:space-between;padding:0 4px">${backBtn('onboarding1')}<span></span></div>
 <div class="ob"><div class="t-large" style="margin:16px 0 22px">${t('onboarding.2.title')}</div>
  <div class="group">${[['import','#007AFF','onboarding.2.step1'],['cube','#34C759','onboarding.2.step2'],['share','#FF9500','onboarding.2.step3']].map(([i,c,k],n)=>
   `<div class="cell has-ic" style="min-height:76px"><span class="ic-badge" style="background:${c};width:44px;height:44px;border-radius:12px">${ic(i)}</span><div class="grow"><div class="t-cap c2">${n+1}</div><div class="t-headline">${t(k)}</div></div></div>`).join('')}</div>
  <div class="banner info" style="margin-top:20px">${ic('lock')}<div>${t('onboarding.2.privacy')}</div></div>
  <div style="flex:1"></div><div class="dots"><i></i><i class="on"></i></div>
  <div style="padding-bottom:44px"><button class="btn btn-primary" data-go="home">${t('common.start')}</button></div></div>${homebar()}</div>`;

const HIST = () => [
  {n:LANG==='en'?'example_bracket.step':'示例_支架.step', f:'step', size:'0.7 MB', when:LANG==='en'?'Today 21:30':'今天 21:30'},
  {n:LANG==='en'?'example_housing.igs':'示例_外壳.igs', f:'iges', size:'3.2 MB', when:LANG==='en'?'Yesterday 18:05':'昨天 18:05'},
  {n:LANG==='en'?'example_gear.step':'示例_齿轮.step', f:'step', size:'12.4 MB', when:LANG==='en'?'Oct 18':'10月18日'},
];
function histRows(list, compact){ return `<div class="group">${list.map(h=>`<div class="cell tap" data-sheet="reshare" style="min-height:62px">
  <span class="hist-ic ${h.f}">${h.f.toUpperCase()}</span><div class="grow"><div class="t-body ellip">${esc(h.n)}</div><div class="t-foot c2 num">${h.when} · ${h.size}</div></div><span class="chev">${ic('chevR')}</span></div>`).join('')}</div>`; }
function homeBody(){ const empty=S.empty; return `
  <div class="dropzone" data-act="pick" role="button" aria-label="${t('home.import')}">
   <div class="big">${ic('import',2.2)}</div><div class="t-title3">${t('home.import')}</div>
   <div class="t-sub c2" style="margin-top:4px">${t('home.import_hint')}</div>
   ${IPAD?`<div class="t-foot c3" style="margin-top:6px">${t('home.drop_hint')}</div>`:''}</div>
  <button class="btn btn-plain" data-sheet="help" style="margin-top:4px">${ic('chat')}${t('home.other_ways')}</button>
  ${empty?`<div class="empty">${ic('clock',1.5)}<div class="t-headline" style="margin-top:10px">${t('home.empty.title')}</div><div class="t-sub c2" style="margin-top:6px">${t('home.empty.body')}</div></div>`
  :`<div style="display:flex;justify-content:space-between;align-items:baseline;margin:18px 4px 8px"><span class="t-title3">${t('home.recent')}</span><a class="t-body ca" data-go="history" style="cursor:pointer">${t('home.see_all')}</a></div>${histRows(HIST())}`}`; }
V.home = () => `<div class="screen">${status()}
 ${nav('', '<span></span>', `<button class="navbtn" data-go="settings" aria-label="${t('home.settings')}">${ic('gear')}</button>`)}
 <div class="content"><div class="largetitle"><div class="t-large">${t('home.title')}</div><div class="t-sub c2">${t('app.tagline')}</div></div>${homeBody()}</div>
 ${sheet()}${homebar()}</div>`;

function viewerHTML(){ const f=FILES[S.file]; return `<div class="viewer" role="img" aria-label="${esc(t('a11y.viewer',{name:fname(f)}))}">
  <canvas id="cv"></canvas><span class="fmtpill pill">${t(f.st.binary?'preview.format.binary':'preview.format.ascii')}</span>
  <div class="tools"><button data-act="resetview" aria-label="${t('preview.reset_view')}">${ic('reset')}</button>
  <button data-act="wire" class="${S.wire?'on':''}" aria-label="${t('preview.wireframe')}">${ic('grid',1.6)}</button></div>
  <div class="hint">${t('preview.gesture_hint')}</div></div>`; }
function statsHTML(){ const s=FILES[S.file].st; const dims=s.size.map(x=>Math.round(x)).join(' × ');
  return `<div class="stats num"><div><div class="k">${t('preview.stats.triangles')}</div><div class="v">${fmtInt(s.faces)}</div></div>
  <div><div class="k">${t('preview.stats.vertices')}</div><div class="v">${fmtInt(s.vertices)}</div></div>
  <div><div class="k">${t('preview.stats.size')}</div><div class="v">${dims}</div></div>
  <div><div class="k">${t('preview.stats.file_size')}</div><div class="v">${fmtBytes(s.bytes)}</div></div></div>
  <div class="section-f" style="margin-left:4px">${t('preview.stats.unit_note')}</div>`; }
function optionsHTML(){ const e=estimate(); const iges=S.fmt==='iges'; const nonAscii=Object.values(S.meta).some(v=>/[^\x00-\x7f]/.test(v));
  const large = e.n > LARGE;
  const pre = k => `<div class="preset ${S.preset===k?'on':''}" role="radio" aria-checked="${S.preset===k}" tabindex="0" data-preset="${k}">
    <div class="pt">${ic({recommended:'star',smaller:'shrink',strict:'shield'}[k])}${t('preset.'+k)}</div><div class="pd">${t('preset.'+k+'.desc')}</div></div>`;
  return `
  ${large?`<div class="banner warn" style="margin-top:12px">${ic('warn')}<div><b>${t('warn.large.title')}</b><br>${t('warn.large.body',{faces:fmtInt(e.n),size:fmtBytes(e.bytes),time:fmtTime(e.sec)})}<br><button class="act" data-act="half">${t('warn.large.action')}</button></div></div>`:''}
  <div class="section-h">${t('export.format')}</div>
  <div class="seg" role="radiogroup"><button class="${!iges?'on':''}" data-fmt="step">${t('export.format.step')}</button><button class="${iges?'on':''}" data-fmt="iges">${t('export.format.iges')}</button></div>
  <div class="section-f">${t(iges?'export.format.iges.desc':'export.format.step.desc')}</div>
  ${iges?`<div class="banner info" style="margin-top:10px">${ic('info')}<div>${t('warn.iges.surfaces')}</div></div>`:''}
  <div class="section-h">${t('preset.title')}${S.preset==='custom'?` <span class="pill accent" style="margin-left:6px;text-transform:none">${t('preset.custom')}</span>`:''}</div>
  <div class="presets" role="radiogroup">${pre('recommended')}${pre('smaller')}${pre('strict')}</div>
  <div class="disclosure ${S.advOpen?'open':''}" data-act="adv" role="button" aria-expanded="${S.advOpen}">${ic('chevR',2.4)}<span class="t-body grow" style="flex:1">${t('advanced.title')}</span><span class="t-foot c2">${S.preset==='custom'?t('preset.custom'):t('preset.'+S.preset)}</span></div>
  ${S.advOpen?advancedHTML(iges,nonAscii):''}`; }
function advancedHTML(iges,nonAscii){ const f=FILES[S.file].st.faces; return `
  <div class="section-h">${t('advanced.simplify')}</div>
  <div class="group"><div class="cell"><div class="grow t-body">${t('advanced.simplify')}</div>${toggle(S.simplify,'simplify')}</div>
   ${S.simplify?`<div class="cell" style="display:block"><div class="t-sub num">${t('advanced.simplify.keep',{pct:S.ratio,faces:fmtInt(f*S.ratio/100)})}</div>
   <input class="slider" type="range" min="5" max="95" step="5" value="${S.ratio}" style="--p:${(S.ratio-5)/90*100}%" data-act="ratio" aria-label="${t('advanced.simplify')}">
   <div class="t-cap c2" style="display:flex;justify-content:space-between"><span>5%</span><span>95%</span></div></div>`:''}</div>
  <div class="section-f">${t('advanced.simplify.desc')}</div>
  <div class="section-h">${t('advanced.mode')}</div>
  <div class="group"><div class="cell" style="display:block"><div class="seg"><button class="${S.mode==='original'?'on':''}" data-mode="original">${t('advanced.mode.original')}</button><button class="${S.mode==='sewing'?'on':''}" data-mode="sewing">${t('advanced.mode.sewing')}</button></div>
   <div class="t-foot c2" style="margin-top:8px">${t(S.mode==='original'?'advanced.mode.original.desc':'advanced.mode.sewing.desc')}</div></div>
   <div class="cell"><div class="grow"><div class="t-body">${t('advanced.merge')}</div><div class="t-foot c2">${t('advanced.merge.desc')}</div></div>${toggle(S.merge,'merge')}</div>
   <div class="cell"><div class="grow"><div class="t-body">${t('advanced.verify')}</div><div class="t-foot c2">${t(iges?'advanced.verify.iges_na':'advanced.verify.desc')}</div></div>${toggle(S.verify&&!iges,'verify',iges)}</div></div>
  <div class="section-h">${t('advanced.meta')}</div>
  <div class="group">${['author','org','product','description'].map(k=>`<div class="field"><label class="t-body" for="m_${k}">${t('advanced.meta.'+k)}</label><input id="m_${k}" data-meta="${k}" placeholder="${t('advanced.meta.placeholder')}" value="${esc(S.meta[k])}"></div>`).join('')}</div>
  ${iges&&nonAscii?`<div class="banner warn" style="margin-top:8px">${ic('warn')}<div>${t('advanced.meta.iges_warn')}</div></div>`:`<div class="section-f">${t('advanced.meta.desc')}</div>`}
  <button class="btn btn-plain" data-act="reset" style="margin-top:8px">${t('advanced.reset')}</button>`; }
function convertBar(){ const e=estimate(); return `<div class="bottombar"><div class="estimate"><span class="num">${t('estimate.line',{time:fmtTime(e.sec),size:fmtBytes(e.bytes)})}</span><span>${t('estimate.note')}</span></div>
  <button class="btn btn-primary" data-act="convert">${ic('bolt')}${t('convert.button',{fmt:S.fmt.toUpperCase()})}</button></div>`; }
V.preview = () => `<div class="screen">${status()}${nav(esc(fname(FILES[S.file])), backBtn('home'), `<button class="navbtn" data-act="pick" aria-label="${t('preview.replace')}">${ic('doc')}</button>`)}
 <div class="content">${viewerHTML()}${statsHTML()}${optionsHTML()}</div>${convertBar()}${homebar()}</div>`;

function stagesList(){ const st=['read']; if (S.simplify) st.push('simplify'); st.push('build','write'); if (S.verify && S.fmt==='step') st.push('verify'); return st; }
const W = {read:10, simplify:10, build:50, write:20, verify:10};
V.converting = () => { const p=S.params.has('p')?+S.params.get('p'):(S.prog||0); const st=stagesList(); const tot=st.reduce((a,k)=>a+W[k],0);
  let acc=0, cur=st.length-1; for (let i=0;i<st.length;i++){ if (p*tot < acc+W[st[i]]) { cur=i; break; } acc+=W[st[i]]; }
  const C=2*Math.PI*74, pct=Math.floor(p*100); const el = (S.params.has('p') ? p*estimate().sec*3 : (S.elapsed||0));
  return `<div class="screen">${status()}${nav(t('converting.title'))}
  <div class="center"><div class="ring" role="progressbar" aria-valuenow="${pct}" aria-label="${t('a11y.progress',{pct})}"><svg viewBox="0 0 168 168"><circle cx="84" cy="84" r="74" fill="none" stroke="var(--fill-2)" stroke-width="12"/>
   <circle cx="84" cy="84" r="74" fill="none" stroke="var(--accent)" stroke-width="12" stroke-linecap="round" stroke-dasharray="${C}" stroke-dashoffset="${C*(1-p)}"/></svg><div class="pct num">${pct}%</div></div>
   <div class="t-headline ellip" style="margin-top:18px;max-width:100%">${esc(fname(FILES[S.file]).replace(/\.stl$/i,'.'+(S.fmt==='step'?'step':'igs')))}</div>
   <div class="t-sub c2 num">${t('converting.elapsed',{t:(el<60?el.toFixed(1)+(LANG==='zh'?' 秒':' s'):fmtTime(el))})}</div>
   <div class="stages">${st.map((k,i)=>`<div class="stage ${i<cur?'done':i===cur?'cur':''}"><span class="dot">${i<cur?ic('check',3):''}</span><span class="t-body">${t('converting.stage.'+k)}</span></div>`).join('')}</div></div>
  <div class="bottombar" style="border:0;background:none"><div class="t-foot c2" style="text-align:center;margin-bottom:10px">${t('converting.keep_open')}</div>
   <button class="btn btn-danger" data-act="askcancel">${ic('stop')}${t('converting.cancel')}</button></div>
  ${S.params.get('confirm')==='1'||S.confirm?`<div class="scrim" style="align-items:center"><div class="alert"><div class="ab"><div class="t-headline">${t('cancel.confirm.title')}</div><div class="t-foot" style="margin-top:4px">${t('cancel.confirm.body')}</div></div>
   <div class="abtns"><button data-act="nocancel" style="font-weight:600">${t('cancel.confirm.no')}</button><button data-act="docancel" style="color:var(--danger)">${t('cancel.confirm.yes')}</button></div></div></div>`:''}
  ${homebar()}</div>`; };

V.done = () => { const e=estimate(); const iges=S.fmt==='iges'; const out=fname(FILES[S.file]).replace(/\.stl$/i, iges?'.igs':'.step');
  const faces = S.merge ? Math.round(e.n*0.1) : e.n; // merge reduction is illustrative only
  return `<div class="screen">${status()}${nav('', '<span></span>', `<button class="navbtn" data-go="home" style="font-weight:600">${t('common.done')}</button>`)}
  <div class="content" style="display:flex;flex-direction:column;align-items:center;padding-top:20px">
   <div class="bigicon ok">${ic('check',2.6)}</div><div class="t-title1">${t('done.title')}</div>
   <div class="resultcard"><div style="display:flex;gap:12px;align-items:center"><span class="hist-ic ${S.fmt}">${S.fmt.toUpperCase()}</span><div class="grow" style="min-width:0"><div class="t-headline ellip">${esc(out)}</div><div class="t-foot c2">${t('common.example')}</div></div></div>
    <div class="kv num"><div><div class="k">${t('done.size')}</div><div class="v">${fmtBytes(e.bytes)}</div></div><div><div class="k">${t('done.time')}</div><div class="v">${e.sec<1?(LANG==='zh'?'0.4 秒':'0.4 s'):fmtTime(e.sec)}</div></div>
    <div><div class="k">${t('done.solids')}</div><div class="v">${iges?'—':'1'}</div></div><div><div class="k">${t('done.faces')}</div><div class="v">${fmtInt(faces)}</div></div></div></div>
   <div style="width:100%;margin-top:10px">${iges?`<div class="banner info">${ic('info')}<div>${t('done.iges_surfaces')}</div></div>`:
     S.verify?`<div class="banner" style="background:color-mix(in srgb,var(--success) 14%,transparent);color:var(--label)"><span class="cs">${ic('shield')}</span><div>${t('done.verify_ok')}</div></div>`:`<div class="t-foot c2">${t('done.verify_off')}</div>`}</div>
   <div style="flex:1;min-height:24px"></div>
   <div class="stack" style="width:100%"><button class="btn btn-primary" data-sheet="share">${ic('share')}${t('done.share')}</button>
    <button class="btn btn-secondary" data-sheet="share">${ic('folder')}${t('done.save')}</button>
    <button class="btn btn-plain" data-go="home">${t('done.another')}</button></div>
   <div class="t-foot c2" style="text-align:center;margin:6px 0 30px">${t('done.tip_pc')}</div></div>${sheet()}${homebar()}</div>`; };

const ERR = { format:['doc','err','error.format',['error.choose_other','pick']], empty:['cube','warn','error.empty',['error.choose_other','pick']],
  too_big:['big','warn','error.too_big',['error.too_big.action','shrinkgo']], memory:['memory','err','error.memory',['error.memory.action','shrinkgo']],
  cancelled:['stop','neutral','error.cancelled',['error.cancelled.action','back']], io:['folder','err','error.io',['common.retry','convert']], generic:['warn','err','error.generic',['common.retry','convert']] };
V.error = () => { const ty=S.params.get('type')||'format'; const [i,c,k,[ak,aa]]=ERR[ty];
  const v={faces:fmtInt(3500000), limit:fmtInt(LIMIT), code:'-8'};
  return `<div class="screen">${status()}${nav('', backBtn('home'))}
  <div class="center"><div class="bigicon ${c}">${ic(i,2)}</div><div class="t-title2">${t(k+'.title')}</div><p class="t-body c2" style="margin:10px 0 0">${t(k+'.body',v)}</p>
  ${ty==='too_big'?`<div class="pill" style="margin-top:12px">${t('common.example')}: ${LANG==='en'?'example_huge.stl':'示例_超大模型.stl'}</div>`:''}</div>
  <div class="bottombar" style="border:0;background:none"><div class="stack"><button class="btn btn-primary" data-act="${aa}">${t(ak)}</button>
  ${ty==='cancelled'?`<button class="btn btn-plain" data-go="home">${t('error.choose_other')}</button>`:ty==='generic'||ty==='memory'?`<button class="btn btn-plain">${t('error.copy_details')}</button>`:`<button class="btn btn-plain" data-go="home">${t('common.back')}</button>`}</div></div>${homebar()}</div>`; };

V.history = () => `<div class="screen">${status()}${nav('', backBtn('home'), `<button class="navbtn" style="color:var(--danger)">${t('history.clear')}</button>`)}
 <div class="content"><div class="largetitle"><div class="t-large">${t('history.title')}</div></div>${histRows(HIST().concat([{n:LANG==='en'?'example_cover.step':'示例_盖板.step',f:'step',size:'1.1 MB',when:LANG==='en'?'Oct 15':'10月15日'}]))}
 <div class="section-f">${t('history.note')}</div><div class="section-f">← ${LANG==='zh'?'左滑可“再次分享”或“删除”':'Swipe left to Share Again or Delete'}</div></div>${sheet()}${homebar()}</div>`;

V.settings = () => { const row=(i,c,k,val,go)=>`<div class="cell has-ic tap" ${go?`data-go="${go}"`:''}><span class="ic-badge" style="background:${c}">${ic(i)}</span><div class="grow t-body">${t(k)}</div>${val?`<span class="val t-body">${val}</span>`:''}<span class="chev">${ic('chevR')}</span></div>`;
 return `<div class="screen">${status()}${nav('', backBtn('home'))}
 <div class="content"><div class="largetitle"><div class="t-large">${t('settings.title')}</div></div>
 <div class="section-h">${t('settings.purchase')}</div><div class="group ph">${row('star','#FF9500','settings.purchase.status',t('settings.purchase.free'),'paywall')}${row('lock','#5856D6','settings.purchase.restore','')}</div>
 <div class="section-f">${t('paywall.placeholder_banner')}</div>
 <div class="section-h">${t('settings.defaults')}</div><div class="group">${row('doc','#007AFF','settings.default_format','STEP')}${row('star','#34C759','settings.default_preset',t('preset.recommended'))}${row('text','#8E8E93','settings.default_meta',t('settings.default_meta.value'))}</div>
 <div class="section-h">${t('settings.appearance')}</div><div class="group">${row('moon','#1C1C1E','settings.appearance',t('settings.appearance.system'))}${row('globe','#30B0C7','settings.language',t('settings.language.value'))}</div>
 <div class="section-h">${t('settings.about')}</div><div class="group">${row('info','#8E8E93','settings.about.version','0.1.0 (1)')}${row('lock','#34C759','settings.about.privacy',t('settings.about.privacy.value'))}${row('doc','#AF52DE','settings.about.licenses','','licenses')}${row('chat','#007AFF','settings.about.feedback','')}</div>
 <div class="section-f">${t('settings.about.engine')}</div>
 <div class="group" style="margin-top:22px"><div class="cell tap" style="justify-content:center;color:var(--danger)">${t('settings.clear_history')}</div></div></div>${homebar()}</div>`; };

V.licenses = () => `<div class="screen">${status()}${nav(t('licenses.title'), backBtn('settings', t('settings.title')))}
 <div class="content" style="padding-top:8px"><div class="group"><div class="cell" style="display:block"><div class="t-headline">${t('licenses.occt.name')}</div><div class="t-foot c2" style="margin:2px 0 8px">${t('licenses.occt.license')}</div>
  <div class="t-sub">${t('licenses.occt.notice')}</div></div><div class="cell tap"><div class="grow t-body ca">${t('licenses.full_text')}</div><span class="chev">${ic('chevR')}</span></div></div>
 <div class="banner warn" style="margin-top:10px">${ic('warn')}<div>${t('licenses.occt.compliance_todo')}</div></div>
 <div class="group" style="margin-top:22px"><div class="cell" style="display:block"><div class="t-headline">${t('licenses.core.name')}</div><div class="t-foot c2">${t('licenses.core.license')}</div></div>
  <div class="cell"><div class="t-sub c2">${t('licenses.others')}</div></div></div></div>${homebar()}</div>`;

V.paywall = () => `<div class="screen">${status()}<div class="nav"><span></span><button class="navbtn" data-go="settings" aria-label="${t('common.close')}">${ic('x')}</button></div>
 <div class="content" style="display:flex;flex-direction:column">
  <div class="banner warn">${ic('warn')}<div><b>${t('common.placeholder_tag')}</b> · ${t('paywall.placeholder_banner')}</div></div>
  <div style="text-align:center;margin:26px 0 14px"><div class="bigicon" style="margin:0 auto 14px;background:var(--accent);color:#fff;border-radius:24px">${ic('cube',2)}</div><div class="t-title1">${t('paywall.title')}</div><div class="t-sub c2" style="margin-top:6px">${t('app.tagline')}</div></div>
  <div class="group" style="padding:6px 16px">${['unlimited','large','iges','meta'].map(k=>`<div class="feat">${ic('check',2.6)}<span class="t-body">${t('paywall.feature.'+k)}</span><span class="pill warn" style="margin-left:auto">${LANG==='zh'?'待定':'TBD'}</span></div>`).join('')}</div>
  <div class="section-f ph" style="margin:14px 4px;padding:6px">${t('paywall.free_limit')}</div>
  <div style="flex:1"></div><div class="stack" style="padding-bottom:34px"><button class="btn btn-primary ph">${t('paywall.buy')}</button>
  <div style="display:flex"><button class="btn btn-plain">${t('paywall.restore')}</button><button class="btn btn-plain" data-go="home" style="color:var(--label-2)">${t('paywall.later')}</button></div>
  <div class="t-cap c3" style="text-align:center">${t('paywall.trigger_note')}</div></div></div>${homebar()}</div>`;

V.ipad = () => `<div class="screen">${status()}<div class="split"><div class="side">
  ${nav('', '<span></span>', `<button class="navbtn" data-go="settings">${ic('gear')}</button>`)}
  <div class="content"><div class="largetitle"><div class="t-large">${t('home.title')}</div><div class="t-sub c2">${t('app.tagline')}</div></div>${homeBody()}</div></div>
  <div class="main">${nav(esc(fname(FILES[S.file])), '<span></span>', `<button class="navbtn" data-act="pick">${ic('doc')}</button>`)}
   <div class="cols"><div style="display:flex;flex-direction:column;min-height:0"><div style="flex:1;min-height:0">${viewerHTML()}</div>${statsHTML()}</div>
   <div class="opts">${optionsHTML()}<div style="margin-top:20px">${convertBar().replace('class="bottombar"','class="" style="padding:0"')}</div></div></div></div></div>${sheet()}</div>`;

/* ---------------- sheets ---------------- */
function sheet(){ if (!S.sheet) return '';
  if (S.sheet==='help') return `<div class="scrim" data-act="closesheet"><div class="sheet" data-stop>
   <div class="grabber"></div><div style="display:flex;justify-content:space-between;align-items:center"><span class="t-title3">${t('import.help.title')}</span><button class="navbtn" data-act="closesheet">${t('common.done')}</button></div>
   <div class="steps" style="margin-top:6px">${[1,2,3].map(n=>`<div class="st"><b>${n}</b><span class="t-body">${t('import.help.step'+n)}</span></div>`).join('')}</div>
   <div class="section-f" style="margin:6px 0 0">${t('import.help.files')}</div></div></div>`;
  if (S.sheet==='share'||S.sheet==='reshare') { const apps=[['airdrop','#0A84FF','share.airdrop'],['chat','#07C160','share.wechat'],['chat','#12B7F5','share.qq'],['mail','#1E90FF','share.mail'],['more','#8E8E93','share.more']];
   return `<div class="scrim" data-act="closesheet"><div class="sheet" data-stop><div class="grabber"></div>
   <div style="display:flex;gap:12px;align-items:center;padding:4px 0 10px;border-bottom:.5px solid var(--separator)"><span class="hist-ic step">STEP</span><div class="grow"><div class="t-headline">${LANG==='en'?'example_bracket.step':'示例_支架.step'}</div><div class="t-foot c2">${LANG==='zh'?'系统分享面板（示意）':'System share sheet (mock)'}</div></div><button class="navbtn" data-act="closesheet">${ic('x')}</button></div>
   <div class="sharegrid">${apps.map(([i,c,k])=>`<div><i style="background:${c}">${ic(i)}</i>${t(k)}</div>`).join('')}</div>
   <div class="group" style="background:var(--surface-2)"><div class="cell has-ic"><span class="ic-badge" style="background:#007AFF">${ic('folder')}</span><div class="grow t-body">${t('share.save_to_files')}</div></div></div></div></div>`; }
  return ''; }

/* ---------------- onboarding art ---------------- */
function artMeshToSolid(){ return `<svg viewBox="0 0 300 190" width="300" height="190" aria-hidden="true">
 <g fill="none" stroke="var(--mesh)" stroke-width="1.4"><path d="M30 60 80 35 130 60 80 85z"/><path d="M30 60v70l50 25V85zM130 60v70l-50 25"/>
 <path d="M30 60 130 60M80 35 80 85M30 95 80 120 130 95M30 95l50-35 50 35M55 72l25 50M105 72 80 120M30 130 80 85 130 130M55 142V97M105 142V97"/></g>
 <path d="M146 95h22" stroke="var(--accent)" stroke-width="3" stroke-linecap="round"/><path d="m162 87 10 8-10 8" fill="none" stroke="var(--accent)" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"/>
 <path d="M190 60 240 35 290 60 240 85z" fill="var(--accent)" opacity=".55"/><path d="M190 60v70l50 25V85z" fill="var(--accent)" opacity=".85"/><path d="M290 60v70l-50 25V85z" fill="var(--accent)"/>
 <text x="80" y="182" text-anchor="middle" font-size="13" fill="var(--label-2)">STL</text><text x="240" y="182" text-anchor="middle" font-size="13" fill="var(--label-2)">STEP / IGES</text></svg>`; }

/* ---------------- 3D viewer (software renderer; real app uses SceneKit / flutter_scene) ---------------- */
const VIEW = {yaw:-0.75, pitch:0.45, zoom:1};
function drawViewer(){ const cv=document.getElementById('cv'); if (!cv) return; const dpr=window.devicePixelRatio||1;
  const w=cv.clientWidth, h=cv.clientHeight; cv.width=w*dpr; cv.height=h*dpr; const g=cv.getContext('2d'); g.setTransform(dpr,0,0,dpr,0,0); g.clearRect(0,0,w,h);
  const T=FILES[S.file].mesh; let mn=[1e9,1e9,1e9],mx=[-1e9,-1e9,-1e9]; for(const tr of T) for(const p of tr) for(let i=0;i<3;i++){mn[i]=Math.min(mn[i],p[i]);mx[i]=Math.max(mx[i],p[i]);}
  const c=mn.map((v,i)=>(v+mx[i])/2), R=Math.hypot(mx[0]-mn[0],mx[1]-mn[1],mx[2]-mn[2])/2, sc=Math.min(w,h)*0.42/R*VIEW.zoom, D=R*4;
  const cy=Math.cos(VIEW.yaw), sy=Math.sin(VIEW.yaw), cp=Math.cos(VIEW.pitch), sp=Math.sin(VIEW.pitch);
  const tf=p=>{ const x=p[0]-c[0], y=p[1]-c[1], z=p[2]-c[2]; const X=x*cy-y*sy, Y0=x*sy+y*cy; const Y=Y0*cp-z*sp, Z=Y0*sp+z*cp; return [X,Y,Z]; };
  const L=[-0.35,-0.75,0.56]; const base=root.dataset.theme==='dark'?[120,150,200]:[110,145,205];
  const polys=[]; for(const tr of T){ const q=tr.map(tf); const u=[q[1][0]-q[0][0],q[1][1]-q[0][1],q[1][2]-q[0][2]], v=[q[2][0]-q[0][0],q[2][1]-q[0][1],q[2][2]-q[0][2]];
    let n=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]; const nl=Math.hypot(...n)||1; n=n.map(x=>x/nl); if (n[1]>0) continue;
    const s2=q.map(p=>{const f=D/(D+p[1]); return [w/2+p[0]*sc*f, h/2-8-p[2]*sc*f];}); const d=(q[0][1]+q[1][1]+q[2][1])/3;
    const lam=Math.max(0,-(n[0]*L[0]+n[1]*L[1]+n[2]*L[2])*-1); const k=0.38+0.62*Math.max(0,n[0]*L[0]+n[1]*L[1]+n[2]*L[2]);
    polys.push([d,s2,k]); }
  polys.sort((a,b)=>b[0]-a[0]);
  for(const [d,s2,k] of polys){ g.beginPath(); g.moveTo(...s2[0]); g.lineTo(...s2[1]); g.lineTo(...s2[2]); g.closePath();
    const col=`rgb(${base.map(x=>Math.round(Math.min(255,x*k+40*k))).join(',')})`; g.fillStyle=col; g.fill();
    g.strokeStyle=S.wire?(root.dataset.theme==='dark'?'rgba(255,255,255,.55)':'rgba(20,40,80,.55)'):col; g.lineWidth=S.wire?0.6:0.8; g.stroke(); }
}
function bindViewer(){ const cv=document.getElementById('cv'); if(!cv) return; const pts=new Map(); let last=null, pinch=null;
  cv.addEventListener('pointerdown',e=>{cv.setPointerCapture(e.pointerId); pts.set(e.pointerId,[e.clientX,e.clientY]); last=[e.clientX,e.clientY];});
  cv.addEventListener('pointermove',e=>{ if(!pts.has(e.pointerId)) return; pts.set(e.pointerId,[e.clientX,e.clientY]);
    if (pts.size===2){ const [a,b]=[...pts.values()]; const d=Math.hypot(a[0]-b[0],a[1]-b[1]); if(pinch) VIEW.zoom=Math.min(4,Math.max(.4,VIEW.zoom*d/pinch)); pinch=d; }
    else { VIEW.yaw+=(e.clientX-last[0])*0.01; VIEW.pitch=Math.max(-1.5,Math.min(1.5,VIEW.pitch+(e.clientY-last[1])*0.01)); last=[e.clientX,e.clientY]; }
    drawViewer(); });
  const up=e=>{pts.delete(e.pointerId); pinch=null;}; cv.addEventListener('pointerup',up); cv.addEventListener('pointercancel',up);
  cv.addEventListener('wheel',e=>{e.preventDefault(); VIEW.zoom=Math.min(4,Math.max(.4,VIEW.zoom*(e.deltaY<0?1.1:0.9))); drawViewer();},{passive:false});
  cv.addEventListener('dblclick',()=>{Object.assign(VIEW,{yaw:-0.75,pitch:0.45,zoom:1}); drawViewer();}); }

/* ---------------- conversion simulation ---------------- */
let timer=null;
function startConvert(){ S.prog=0; S.elapsed=0; S.confirm=false; go('converting'); clearInterval(timer); const t0=Date.now(); const dur=S.file==='large'?6000:3200;
  timer=setInterval(()=>{ if (S.confirm) return; S.prog=Math.min(1,S.prog+50/dur); S.elapsed=(Date.now()-t0)/1000; if(S.route==='converting'&&!S.params.has('p')) render();
    if (S.prog>=1){ clearInterval(timer); go('done'); } },50); }

/* ---------------- events ---------------- */
document.addEventListener('click',e=>{ const el=e.target.closest('[data-go],[data-act],[data-fmt],[data-preset],[data-mode],[data-sheet]'); if(!el) return;
  if (el.hasAttribute('data-stop') && !e.target.closest('[data-act]')) return;
  if (el.dataset.go){ clearInterval(timer); go(el.dataset.go); return; }
  if (el.dataset.sheet){ S.sheet=el.dataset.sheet; render(); return; }
  if (el.dataset.fmt){ S.fmt=el.dataset.fmt; render(); return; }
  if (el.dataset.preset){ setPreset(el.dataset.preset); render(); return; }
  if (el.dataset.mode){ S.mode=el.dataset.mode; markCustom(); render(); return; }
  const a=el.dataset.act;
  if (a==='closesheet'){ if (e.target.closest('.sheet') && !e.target.closest('[data-act="closesheet"]')) return; S.sheet=null; render(); }
  else if (a==='pick'){ go('preview','file=bracket'); }
  else if (a==='adv'){ S.advOpen=!S.advOpen; render(); }
  else if (['simplify','merge','verify'].includes(a)){ S[a]=!S[a]; markCustom(); render(); }
  else if (a==='half'){ S.simplify=true; S.ratio=50; S.advOpen=true; markCustom(); render(); }
  else if (a==='reset'){ setPreset('recommended'); S.meta={author:'',org:'',product:'',description:''}; render(); }
  else if (a==='resetview'){ Object.assign(VIEW,{yaw:-0.75,pitch:0.45,zoom:1}); drawViewer(); }
  else if (a==='wire'){ S.wire=!S.wire; el.classList.toggle('on',S.wire); drawViewer(); }
  else if (a==='convert'){ startConvert(); }
  else if (a==='askcancel'){ S.confirm=true; render(); }
  else if (a==='nocancel'){ S.confirm=false; if (S.params.has('confirm')) go('converting','p=0.62'); else render(); }
  else if (a==='docancel'){ clearInterval(timer); S.confirm=false; go('error','type=cancelled'); }
  else if (a==='back'){ go('preview'); }
  else if (a==='shrinkgo'){ S.file='large'; S.simplify=true; S.ratio=20; S.advOpen=true; markCustom(); go('preview'); }
});
document.addEventListener('input',e=>{ const el=e.target;
  if (el.dataset.act==='ratio'){ S.ratio=+el.value; markCustom(); const y=document.querySelector('.content').scrollTop; render(); document.querySelector('.content').scrollTop=y; document.querySelector('[data-act=ratio]').focus(); }
  if (el.dataset.meta){ S.meta[el.dataset.meta]=el.value; const warnNeeded=S.fmt==='iges'&&Object.values(S.meta).some(v=>/[^\x00-\x7f]/.test(v)); if (warnNeeded!==S._w){ S._w=warnNeeded; const y=document.querySelector('.content').scrollTop; render(); document.querySelector('.content').scrollTop=y; const i=document.getElementById(el.id); i.focus(); i.setSelectionRange(i.value.length,i.value.length);} }
});
document.addEventListener('keydown',e=>{ if((e.key===' '||e.key==='Enter')&&e.target.matches('[role=switch],[role=radio],[role=button]')){ e.preventDefault(); e.target.click(); } });

/* ---------------- harness ---------------- */
const SCREENS=[['onboarding1','引导 1'],['onboarding2','引导 2'],['home','首页'],['home?empty=1','首页（空）'],['home?sheet=help','微信导入说明'],['preview','预览 + 转换'],['preview?adv=1','高级设置'],['preview?file=large','大模型提醒'],['preview?fmt=iges&adv=1&meta=zh','IGES + 中文提醒'],['converting','转换中（实时）'],['converting?p=0.62&confirm=1','取消确认'],['done','完成'],['done?sheet=share','分享面板'],['error?type=format','错误：文件无效'],['error?type=too_big','错误：太大'],['error?type=memory','错误：内存不足'],['error?type=cancelled','已取消'],['history','转换记录'],['settings','设置'],['licenses','开源许可'],['paywall','购买（占位）']];
function harness(){ if (Q.get('bare')==='1') return ''; const cur=location.hash.replace('#/','')||'home';
  return `<div class="panel"><span class="tag">原型 · 所有数值均为示例</span><h3>STL2STP 界面原型</h3>
  <div class="row"><button data-theme="light" class="${root.dataset.theme==='light'?'on':''}">浅色</button><button data-theme="dark" class="${root.dataset.theme==='dark'?'on':''}">深色</button>
  <button data-lang="${LANG==='zh'?'en':'zh'}">${LANG==='zh'?'English':'中文'}</button></div>
  <div class="row"><a href="?device=ipad#/ipad" style="background:var(--fill)">iPad 布局</a><a href="?text=xl#/preview" style="background:var(--fill)">大字号</a><a href="index.html#/home" style="background:var(--fill)">iPhone</a></div>
  ${SCREENS.map(([r,n])=>`<a href="#/${r}" class="${cur===r?'on':''}">${n}</a>`).join('')}</div>`; }
document.addEventListener('click',e=>{ const b=e.target.closest('.panel button'); if(!b) return;
  if (b.dataset.theme){ applyTheme(b.dataset.theme); render(); } if (b.dataset.lang){ LANG=b.dataset.lang; render(); } });

function render(){ const app=document.getElementById('app'); const r=IPAD?'ipad':(S.route in V?S.route:'home');
  app.innerHTML=`<div class="harness">${harness()}<div class="device ${IPAD?'ipad':''}">${V[r]()}</div></div>`;
  document.documentElement.lang = LANG==='zh'?'zh-Hans':'en'; drawViewer(); bindViewer(); }
window.addEventListener('resize',drawViewer);
if (!location.hash) location.hash = localStorage.getItem('stl2stp_onboarded') ? '#/home' : '#/onboarding1';
try{ localStorage.setItem('stl2stp_onboarded','1'); }catch(_){}
route();
})();
