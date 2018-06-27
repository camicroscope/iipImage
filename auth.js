const express = require('express')
var rp = require('request-promise');
var app = express();


var iipbase = "http://localhost";


app.use("/iip", function(req, res){
  console.log(req.headers)
  if (!req.headers.authorization) {
    return res.status(401).json({ error: 'No authorization header set' });
  }
  var path = iipbase + "/" + req.originalUrl.split("/").splice(2).join("/")
  console.log(path)
  options = {
    uri: path,
    method: req.method,
    resolveWithFullResponse: true
  }
  var resource = rp(options);
  resource.then(response=>{
    res.set(response.headers)
    console.log(Object.keys(response))
    res.send(response.body)}
  );
  resource.catch(e=>res.send(e))
})

app.listen(4010, () => console.log('listening on 4010'))
