var http = require("http");
var url = require("url");
var path = require("path");
var fs  = require("fs");
const port = 10720;

function check_header(request, header, value) {
	if (!request.headers[header.toLowerCase()]) return false;
	if (value && request.headers[header.toLowerCase()] !== value)	return false;
	return true;
}

var available_version = "00000";

var server = http.createServer(function(request,response) {
	var d = new Date();
	var TimeZone = (new Date().getTimezoneOffset()) * -60000;			// get time zone and convert to ms
	var datetime = new Date(d.getTime() + TimeZone);
	
	console.log ("[" + d + "]: OTA request");
	var inValidtxt = "";
	if (!check_header(request, 'USER-AGENT', 'ESP8266-http-Update')) inValidtxt += "USER-AGENT  ";
	if (!check_header(request, 'X-ESP8266-STA-MAC')) inValidtxt += "X-ESP8266-STA-MAC  ";
	if (!check_header(request, 'X-ESP8266-AP-MAC')) inValidtxt += "X-ESP8266-AP-MAC  ";
	if (!check_header(request, 'X-ESP8266-FREE-SPACE')) inValidtxt += "X-ESP8266-FREE-SPACE  ";
	if (!check_header(request, 'X-ESP8266-SKETCH-SIZE')) inValidtxt += "X-ESP8266-SKETCH-SIZE  ";
	//if (!check_header(request, 'X-ESP8266-SKETCH-MD5')) inValidtxt += "X-ESP8266-SKETCH-MD5  ";
	if (!check_header(request, 'X-ESP8266-CHIP-SIZE')) inValidtxt += "X-ESP8266-CHIP-SIZE  ";
	if (!check_header(request, 'X-ESP8266-SDK-VERSION')) inValidtxt += "X-ESP8266-SDK-VERSION  ";
	
/*	if (!check_header(request, 'USER-AGENT', 'ESP8266-http-Update') ||
		!check_header(request, 'X-ESP8266-STA-MAC') ||
		!check_header(request, 'X-ESP8266-AP-MAC') ||
		!check_header(request, 'X-ESP8266-FREE-SPACE') ||
    !check_header(request, 'X-ESP8266-SKETCH-SIZE') ||
		!check_header(request, 'X-ESP8266-SKETCH-MD5') ||
    !check_header(request, 'X-ESP8266-CHIP-SIZE') ||
    !check_header(request, 'X-ESP8266-SDK-VERSION' ||
		!check_header(request, "X-ESP8266-VERSION"))) {  */
	if (inValidtxt.length > 0) {
		response.writeHeader(403, {"Content-Type": "text/plain"});
		response.write("403 Forbidden");
		response.end();
		console.log ("  => [Error] invalid request header.." + inValidtxt);
	}	else {
		// check version get from 
		// t_httpUpdate_return ret = ESPhttpUpdate.update(OTA host, OTA port, OTA URI, "X-ESP8266-VERSION value");
		// reserve for future use
		/*if (check_header(request, "X-ESP8266-VERSION", available_version)) {
			response.writeHeader(304, {"Content-Type": "text/plain"});
			response.write("304 Not Modified\n");  
			response.end();
			console.log ("[FOTA] X-ESP8266-VERSION mismatch");
		}	else {  */
			var my_path = url.parse(request.url).pathname;
			var full_path = path.join(process.cwd(), my_path);
			console.log ("  => [INFO] Device ID: " + request.headers['x-esp8266-version']);
			console.log ("  => [INFO] FOTA file: " + my_path);
			fs.exists(full_path,function(exists){
				if (!exists) {
					response.writeHeader(404, {"Content-Type": "text/plain"});
					response.write("404 Not Found\n");  
					response.end();
					console.log ("  => [FOTA] Error: file not found: " + full_path);
				}	else {
					fs.readFile(full_path, "binary", function(err, file) {  
						if (err) {  
							response.writeHeader(500, {"Content-Type": "text/plain"});
							response.write(err + "\n");
							response.end();
							console.log ("  => [FOTA] Error: file read error: " + full_path);
						} else {
							response.setTimeout(180000);
							response.writeHeader(200, 
								{"Cache-Control": "no-cache",
								 "Pragma": "no-cache",
								 "Content-Transfer-Encoding": "binary",
								 "Content-Type": "application/octet-stream", 
								 "Content-Disposition": "attachment;filename=" + path.basename(full_path),
								 "Content-Length": "" + fs.statSync(full_path)["size"]});
							response.write(file, "binary");  
							response.end();
							console.log ("  => [FOTA] Success: OTA file sent.");
						}
					});				
				}
			});
		//}
	}
});

server.listen(port, () => {
  console.log("ESP FOTA Server running at port " + port);
});