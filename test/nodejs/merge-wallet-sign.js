var http = require('http');
var querystring = require('querystring');


var g_host='52.80.12.8';
var g_port = 6002;
var g_address = "buQs9npaCq9mNFZG18qu88ZcmXYqd6bqpTU3";
var g_priv_key = "privbvYfqQyG3kZyHE4RX4TYVa32htw8xG4WdpCTrymPUJQ923XkKVbM";
var g_dest = "";
var g_nonce= 0;
var global = 0;
var g_contract_address = "";
var fs = require('fs');

let test1 = JSON.parse("{\"items\":[{\"transaction_blob\":\"0a2462755157586e6a36415a715434517948374569374754457836514c7741677a5573737048100218c096b10220e4193a330807522f0a246275517373466e6261713646556f4354485856533644474b6d336e7555687179334a3637108080cef2be8faa05\",\"signatures\":[{\"sign_data\":\"246b5f8d878aa0b338a7bb499cba060b1923dd7c27ba17ffaf27a37426795c0a00b860517c611230bad46534c6da8b5a2ffafdd6ff8b455be7d9b30eef026c00\",\"public_key\":\"b001b746151b1dcb344024a85610e4f113ad9a5fdd5c90216df2d845559c92ba75ebf440b84d\"}]}]}");
var contents_json = [];

let content1 = fs.readFileSync("merge-wallet-sign.txt", 'UTF-8');
let cont_split = content1.split("|");
for (let index = 0; index < cont_split.length; index++) {
	let element = cont_split[index];
	element = element.replace(/\r\n/g, "");
	let json_ele = hex2a(element);

	contents_json.push(JSON.parse(json_ele));
}


let json1 = contents_json[0];
for(var i = 1; i < contents_json.length; i++){
	let sig = contents_json[i].items[0].signatures[0];
	json1.items[0].signatures.push(sig);
}

let str_json1 = a2hex(JSON.stringify(json1));
console.log(str_json1);


function hex2a(hexx) {
    var hex = hexx.toString();//force conversion
    var str = '';
    for (var i = 0; i < hex.length; i += 2)
        str += String.fromCharCode(parseInt(hex.substr(i, 2), 16));
    return str;
}

function a2hex(str) {
	var arr = [];
	for (var i = 0, l = str.length; i < l; i ++) {
	  var hex = Number(str.charCodeAt(i)).toString(16);
	  arr.push(hex);
	}
	return arr.join('');
  }