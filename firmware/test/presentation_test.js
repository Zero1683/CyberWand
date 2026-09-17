const assert=require('node:assert/strict');
const {visibleLabel}=require('../../studio/static/presentation.js');
const state={connected:true,ready:true,history:[{label:'left',timestamp:100}]};
assert.equal(visibleLabel(state,100000),'left');
assert.equal(visibleLabel(state,102999),'left');
assert.equal(visibleLabel(state,103000),null);
assert.equal(visibleLabel({...state,capturing:true},101000),null);
assert.equal(visibleLabel({...state,connected:false},101000),null);
assert.equal(visibleLabel({...state,ready:false},101000),null);
assert.equal(visibleLabel({...state,history:[{label:'unknown',timestamp:100}]},101000),null);
assert.equal(visibleLabel({...state,history:[]},101000),null);
console.log('PASS: idle, recognized, 3s expiry, capturing, disconnected, calibration, unknown');

assert.equal(visibleLabel({...state,history:[{label:'custom6',timestamp:100}]},101000),'custom6');
assert.equal(visibleLabel({...state,mode:'custom1'},101000),null);
assert.equal(visibleLabel({...state,history:[{label:'custom7',timestamp:100}]},101000),null);
