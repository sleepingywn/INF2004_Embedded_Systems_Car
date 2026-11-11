"use strict";
const fs=require('node:fs'),path=require('node:path'),esbuild=require('esbuild');
async function build(){
 const dir=path.join(__dirname,'public','robocar');fs.mkdirSync(dir,{recursive:true});
 for(const name of ['index.html','overview.css'])fs.copyFileSync(path.join(__dirname,'web',name),path.join(dir,name));
 fs.copyFileSync(path.join(__dirname,'node_modules','three','LICENSE'),path.join(dir,'THREE-LICENSE.txt'));
 const fontDir=path.join(dir,'fonts');fs.mkdirSync(fontDir,{recursive:true});
 for(const [family,weights] of [['chakra-petch',[400,500,600]],['ibm-plex-mono',[400]]]){
  const source=path.join(__dirname,'node_modules','@fontsource',family);
  for(const weight of weights){const name=`${family}-latin-${weight}-normal.woff2`;fs.copyFileSync(path.join(source,'files',name),path.join(fontDir,name));}
  fs.copyFileSync(path.join(source,'LICENSE'),path.join(fontDir,`${family}-LICENSE.txt`));
 }
 await esbuild.build({entryPoints:[path.join(__dirname,'web','overview.js')],bundle:true,minify:true,
  outfile:path.join(dir,'overview.js'),platform:'browser',target:'es2022',legalComments:'linked'});
 const flowPath=path.join(__dirname,'flows.json'),flows=JSON.parse(fs.readFileSync(flowPath,'utf8'));
 flows.find(n=>n.id==='view').format=fs.readFileSync(path.join(__dirname,'overview-template.vue'),'utf8');
 fs.writeFileSync(flowPath,JSON.stringify(flows,null,2)+'\n');
 console.log('Built offline Overview assets and Node-RED template.');
}
build().catch(error=>{console.error(error);process.exitCode=1;});
