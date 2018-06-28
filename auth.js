const express = require('express')
var rp = require('request-promise');
var app = express();


var iipbase = "http://localhost";


app.use("/", function(req, res){
  // skip this check if told to
  var skip_check = process.env.CHECK_HEADER=="no"
  if (!skip_check && !req.headers.authorization) {
    return res.status(401).json({ error: 'No authorization header set' });
  }
  var path = iipbase + "/" + req.originalUrl.split("/").splice(2).join("/")
  options = {
    uri: path,
    method: req.method,
    resolveWithFullResponse: true
  }
  var resource = rp(options);
  resource.then(response=>{
    res.set(response.headers)
    res.send(response.body)}
  );
  resource.catch(e=>res.send(e))
})

app.listen(4010, () => console.log('listening on 4010'))
