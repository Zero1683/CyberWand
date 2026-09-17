(function(root){
  function visibleLabel(state,now=Date.now()) {
    if(!state.connected||state.stale||!state.ready||state.capturing||(state.mode&&state.mode!=='recognize'))return null;
    const item=state.history?.[0],allowed=['left','right','up','down','circle','zigzag',...Array.from({length:6},(_,i)=>'custom'+(i+1))];
    if(!item||!allowed.includes(item.label))return null;
    const age=now-Number(item.timestamp||0)*1000;
    return age>=0&&age<3000?item.label:null;
  }
  const api={visibleLabel};if(typeof module!=='undefined'&&module.exports)module.exports=api;else root.WandPresentation=api;
})(typeof window!=='undefined'?window:globalThis);
