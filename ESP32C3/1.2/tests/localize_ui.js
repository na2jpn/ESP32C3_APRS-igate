const texts=require('../ui_texts.json');
module.exports=(source,lang)=>{
  for(const [key,value] of Object.entries(texts))source=source.split('{{'+key+'}}').join(value[lang]);
  if(/\{\{[a-z][a-z0-9_.]*\}\}/.test(source))throw Error('Unresolved translation marker');
  return source;
};
