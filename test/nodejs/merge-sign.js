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

var files = ["merge-sign/sr_gq.json","merge-sign/sr_jh.json","merge-sign/sr_wj.json","merge-sign/sr_zzy.json"];
var contents_json = {};


for( var file_item = 0; file_item < files.length; file_item++ ){
	let content1 = fs.readFileSync(files[file_item], 'UTF-8');
	let content1_json = JSON.parse(content1);
	let items = content1_json.items;

	var cont_id = [];
	for(var i = 0; i < items.length; i++){
		let key = items[i].transaction_blob;
		if( contents_json[key] == undefined ) contents_json[key] = [];
		contents_json[key].push(items[i].signatures[0]);
		cont_id.push(key);
	}
}

var contens = {}; 
for(let z1 in contents_json){
	let item = {
		"items" :[
			{
				"signatures" : [],
				"transaction_blob":""
			}
		]
	};
	item.items[0].transaction_blob = z1;
	item.items[0].signatures = contents_json[z1];
	contens[z1] = JSON.stringify(item);
	//console.log(JSON.stringify(item));
}

for (let m=0; m < cont_id.length; m++){
	console.log(contens[cont_id[m]]);
}