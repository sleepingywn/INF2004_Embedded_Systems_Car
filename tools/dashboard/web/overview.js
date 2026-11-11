import * as THREE from 'three';
import {GLTFLoader} from 'three/examples/jsm/loaders/GLTFLoader.js';
import {OrbitControls} from 'three/examples/jsm/controls/OrbitControls.js';
import {RoomEnvironment} from 'three/examples/jsm/environments/RoomEnvironment.js';
import {PoseTracker} from '../pose.js';

const root=document.getElementById('robo-overview'),host=document.getElementById('car-stage');
const tracker=new PoseTracker(),radToDeg=180/Math.PI;
const states=['Idle','Driving','Travelling','Turning','Following line','Reacquiring line','Fault','Bench pulse'];
const faultNames=['Encoder','Stall','Timeout','IR','Handoff','Range','External','Hardware'];
let renderer,camera,controls,scene,pose,wheelL,wheelR,headingArrow,segment=0;
let from={...tracker.pose},target={...tracker.pose},shown={...tracker.pose},transitionAt=0;
const reducedMotion=window.matchMedia('(prefers-reduced-motion: reduce)');
const el=id=>document.getElementById(id);
const text=(id,value)=>{const e=el(id);if(e.textContent!==String(value))e.textContent=value;};
const number=(v,digits=0)=>v==null||!Number.isFinite(v)?'—':v.toFixed(digits);
const angles={perspective:[-.33,.24,-.40],top:[0,.68,.001],front:[0,.13,-.53],side:[-.53,.13,0]};
const focus=new THREE.Vector3(0,.075,0);
function setView(name){
 if(!camera||!angles[name])return;
 controls.target.copy(focus);camera.position.copy(focus).add(new THREE.Vector3(...angles[name]));controls.update();el('camera-view').value=name;
 try{localStorage.setItem('robocar-camera-view',name);}catch{}
}
function renderReadings(){
 const s=tracker.snapshot(),d=s.packet||{};
 const valid=bit=>s.fresh&&!!(d.valid&bit);
 const l=s.speed.left,r=s.speed.right,mean=l==null||r==null?null:(Math.abs(l)+Math.abs(r))/2;
 root.classList.toggle('stale',!s.fresh);root.classList.toggle('fault',valid(16)&&!!d.faults);
 text('connection',!s.packet?'Waiting for telemetry':s.fresh?'Receiving telemetry':'Telemetry stale');
 text('age',s.packet?(s.age/1000).toFixed(1)+' s since receipt':'No data received');
 text('mean',number(mean));text('left-speed',number(l));text('right-speed',number(r));
 el('speed-gauge').setAttribute('stroke-dasharray',(mean==null?0:Math.min(mean/800,1)*306)+' 409');
 el('left-meter').style.width=(l==null?0:Math.min(Math.abs(l)/800,1)*100)+'%';el('right-meter').style.width=(r==null?0:Math.min(Math.abs(r)/800,1)*100)+'%';
 text('pitch',s.attitudeValid?number(d.pitch,1)+'°':'—');text('roll',s.attitudeValid?number(d.roll,1)+'°':'—');
 text('attitude-status',s.attitudeValid?'Live pitch and roll':'IMU unavailable · last pose held');
 el('level-marker').style.left=(s.attitudeValid?Math.max(5,Math.min(95,50+d.roll*1.5)):50)+'%';el('level-marker').style.opacity=s.attitudeValid?1:0;
 const heading=-s.pose.yaw*radToDeg;
 text('heading',s.motionValid?'Est. turn '+number(heading,1)+'°':'Heading held · no fresh counts');
 text('pose-quality',s.continuous?'Relative heading · provisional wheel calibration':'Heading estimate interrupted by missing data');
 text('state',!s.packet?'Waiting for telemetry':!s.fresh?'Connection stale · pose held':valid(16)?(states[d.state]||'Unknown motor state'):'Motor state unavailable');
 text('motion-status',!s.motionValid?'Encoder data unavailable':l==null?'Collecting count baseline':'Measured wheel feedback');
 text('line-error',valid(4)?number(d.lineError):'—');el('error-marker').style.left='calc('+Math.max(0,Math.min(100,50+(d.lineError||0)/2))+'% - 4px)';el('error-marker').style.opacity=valid(4)?1:0;
 text('hump-count',valid(8)?number(d.humpCount):'—');text('hump-height',valid(8)?number(d.highestPeakMm)+' mm':'—');
 text('faults',valid(16)?d.faults===0?'None':faultNames.filter((_,i)=>d.faults&(1<<i)).join(', '):'Unavailable');
 text('ready',valid(16)?d.ready?'Yes':'No':'Unavailable');
 text('metric-speeds','Calibrated speed: left '+(valid(3)&&d.leftSpeed!=null?d.leftSpeed+' mm/s':'unavailable')+' · right '+(valid(3)&&d.rightSpeed!=null?d.rightSpeed+' mm/s':'unavailable'));
 text('freshness-note',!s.packet?'Waiting for the first live packet.':s.fresh?'Model follows received encoder and IMU samples.':'No fresh data · motion frozen; last packet retained below.');
}
function accept(payload){
 if(!tracker.accept(payload))return;
 const s=tracker.snapshot();
 from={...shown};target={...s.pose};transitionAt=performance.now();
 if(s.segment!==segment){segment=s.segment;from={...target};shown={...target};}
 text('diagnostics',JSON.stringify(payload,null,2));renderReadings();
}
window.addEventListener('message',event=>{
 if(event.origin!==location.origin||event.source!==window.parent||event.data?.type!=='robocar-telemetry')return;
 accept(event.data.payload);
});
function reportSize(){if(window.parent!==window)window.parent.postMessage({type:'robocar-resize',height:Math.ceil(root.getBoundingClientRect().height)+4},location.origin);}
new ResizeObserver(reportSize).observe(root);
window.parent.postMessage({type:'robocar-ready'},location.origin);
renderReadings();setInterval(renderReadings,250);
el('camera-view').addEventListener('change',e=>setView(e.target.value));
el('reset-camera').addEventListener('click',()=>setView('perspective'));
for(const[id,sign]of [['orbit-left',-1],['orbit-right',1]])el(id).addEventListener('click',()=>{
 if(!camera)return;const offset=camera.position.clone().sub(controls.target);offset.applyAxisAngle(new THREE.Vector3(0,1,0),sign*Math.PI/6);camera.position.copy(controls.target).add(offset);controls.update();el('camera-view').value='free';
});
async function init(){
 try{
  const gltf=await new GLTFLoader().loadAsync('robocar.glb');
  renderer=new THREE.WebGLRenderer({alpha:true,antialias:true});renderer.setPixelRatio(Math.min(devicePixelRatio,1.5));renderer.toneMapping=THREE.ACESFilmicToneMapping;renderer.toneMappingExposure=1.35;
  scene=new THREE.Scene();camera=new THREE.PerspectiveCamera(35,1,.005,5);
  const env=new RoomEnvironment(),pmrem=new THREE.PMREMGenerator(renderer);scene.environment=pmrem.fromScene(env,.04).texture;env.dispose();pmrem.dispose();
  scene.add(new THREE.HemisphereLight(0xe9ffe4,0x1d3424,2));const key=new THREE.DirectionalLight(0xffffff,3);key.position.set(-1,2,-1);scene.add(key);const rim=new THREE.DirectionalLight(0xb5e2b9,2);rim.position.set(1,1,1);scene.add(rim);
  pose=new THREE.Group();pose.position.y=.033;scene.add(pose);gltf.scene.position.y=-.033;pose.add(gltf.scene);
  wheelL=gltf.scene.getObjectByName('Wheel_Left');wheelR=gltf.scene.getObjectByName('Wheel_Right');if(!wheelL||!wheelR)throw new Error('Wheel pivots missing');
  const grid=new THREE.GridHelper(.64,20,0x49634f,0x304334);grid.position.y=-.002;scene.add(grid);
  const reference=new THREE.ArrowHelper(new THREE.Vector3(0,0,-1),new THREE.Vector3(0,.003,-.15),.07,0x6a806d,.018,.012);scene.add(reference);
  headingArrow=new THREE.Group();headingArrow.add(new THREE.ArrowHelper(new THREE.Vector3(0,0,-1),new THREE.Vector3(0,.007,-.15),.07,0xb6de9f,.022,.018));scene.add(headingArrow);
  host.replaceChildren(renderer.domElement);
  controls=new OrbitControls(camera,renderer.domElement);controls.enablePan=false;controls.enableDamping=false;controls.minDistance=.28;controls.maxDistance=.95;controls.minPolarAngle=.001;controls.maxPolarAngle=Math.PI/2-.05;controls.zoomSpeed=.7;controls.rotateSpeed=.6;
  controls.addEventListener('start',()=>{el('camera-view').value='free';});
  let saved='perspective';try{saved=localStorage.getItem('robocar-camera-view')||saved;}catch{}setView(angles[saved]?saved:'perspective');
  const resize=()=>{if(!host.clientWidth||!host.clientHeight)return;renderer.setSize(host.clientWidth,host.clientHeight);camera.aspect=host.clientWidth/host.clientHeight;camera.zoom=Math.min(1.18,camera.aspect/1.08);camera.updateProjectionMatrix();};new ResizeObserver(resize).observe(host);resize();
  root.dataset.modelReady='true';
  function frame(now){
   const s=tracker.snapshot();const f=reducedMotion.matches?1:Math.min(1,(now-transitionAt)/150);
   // Interpolate only to observed positions. Never extrapolate wheel motion.
   for(const k of ['left','right','yaw'])if(s.motionValid)shown[k]=from[k]+(target[k]-from[k])*f;
   for(const k of ['pitch','roll'])if(s.attitudeValid)shown[k]=from[k]+(target[k]-from[k])*f;
   pose.rotation.set(shown.pitch,shown.yaw,shown.roll,'YXZ');wheelL.rotation.x=shown.left;wheelR.rotation.x=shown.right;headingArrow.rotation.y=shown.yaw;
   controls.update();renderer.render(scene,camera);requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);
 }catch(error){text('model-status','3D view unavailable. Telemetry readings remain available.');console.error(error);}
}
init();
