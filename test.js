#!/usr/local/bin/node

var libzfs = require('./build/Release/libzfs') ;
var handle = new libzfs.zfshandle();
//console.log("version " + handle.version());
var listObj = handle.list();
console.log(JSON.stringify(listObj)) ;

