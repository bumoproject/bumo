"use strict";

const crtKey = 'contract';

const error = {
  LOG_ADDR_ERR: {
    code: 19994,
    msg: 'The logic address is invalid.'
  },
  NOT_CRT_ERR: {
    code: 19995,
    msg: 'The register interface must be called first.'
  },
  REG_ERR: {
    code: 19997,
    msg: 'The register interface is already called.'
  }
};

function init() {
  return;
}

function main(input) {
  let data = JSON.parse(input);
  const method = data.method || '';
  let params = data.params || {};
  const crtVal = Chain.load(crtKey);
  if (method !== 'register') {
    Utils.assert(crtVal !== false, JSON.stringify(error.NOT_CRT_ERR));
    const crt = JSON.parse(crtVal);
    params.logicAddress = crt.logicAddress;
    data.params = params;
  } else {
    Utils.assert(crtVal === false, JSON.stringify(error.REG_ERR));
    Utils.assert(Utils.addressCheck(params.logicAddress) && Utils.int64Compare(Chain.getBalance(params.logicAddress), 0) > 0, JSON.stringify(error.LOG_ADDR_ERR));
  }
  
  Chain.delegateCall(params.logicAddress, JSON.stringify(data));
}

function query(input) { 
  const crtVal = Chain.load(crtKey);
  Utils.assert(crtVal !== false, JSON.stringify(error.NOT_CRT_ERR));
  const crt = JSON.parse(crtVal);
  return Chain.delegateQuery(crt.logicAddress, input);
}