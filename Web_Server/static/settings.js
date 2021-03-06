/*****************************************************************************
          File: settings.js
   Description: Implements the settings page functions for GarageRTC.  See
                https://github.com/jharmer95/Garage-RTC/ for details on the
                Open GarageRTC project.
       Authors: Daniel Zajac,  danzajac@umich.edu
                Jackson Harmer, jharmer@umich.edu

*****************************************************************************/
// Create socket
var socket = io.connect("http://" + document.domain + ":" + location.port);
var time = 0;

$(document).ready(function () {
    socket.emit("getSettings");
});

// function to submit the settings form and send the data back to the server
function submitForm() {
    var ipForm = document.getElementById("udpIP");
    var ip = ipForm.value;

    var sIP = ip.split(".");
    var isValid = sIP.length == 4;

    if (isValid) {
        for (var i = 0; i < 4; ++i) {
            var nIP = parseInt(sIP[i]);
            if (nIP > 255 || nIP < 0) {
                isValid = false;
                break;
            }
        }
    }

    if (isValid) {
        socket.emit("setSettings", [{"name" : "udpIP", "value": ip}]);
        $(cycleTimeForm).addClass("validText").removeClass("invalidText");
        document.getElementById("errorMsg").innerHTML = "";
    }
    else {
        $(ipForm).addClass("invalidText").removeClass("validText");
        document.getElementById("errorMsg").innerHTML = "Please enter a valid IPv4 address!";
    }

    var portForm = document.getElementById("udpPort");
    var port = parseInt(portForm.value);
    console.log(port);

    if (port > 0 && port < 65536) {
        socket.emit("setSettings", [{"name" : "udpPort", "value": portForm.value}]);
        $(cycleTimeForm).addClass("validText").removeClass("invalidText");
        document.getElementById("errorMsg").innerHTML = "";
    }
    else {
        $(ipForm).addClass("invalidText").removeClass("validText");
        document.getElementById("errorMsg").innerHTML = "Please enter a valid port!";
    }

    var cycleTimeForm = document.getElementById("screenCycle");
    var cycleTime = cycleTimeForm.value;

    if (!isNaN(cycleTime) && cycleTime.indexOf('.') == -1) {
        socket.emit("setSettings", [{"name" : "screenCycle", "value": cycleTime}]);
        $(cycleTimeForm).addClass("validText").removeClass("invalidText");
        document.getElementById("errorMsg").innerHTML = "";
    }
    else {
        $(cycleTimeForm).addClass("invalidText").removeClass("validText");
        document.getElementById("errorMsg").innerHTML = "Cycle time must be an integer!";
    }
}

// Handles the updateSettings event to change the values shown on screen to reflect the new settings
socket.on("updateSettings", function (msg) {
    objs = JSON.parse(msg);
    for (var i = 0; i < objs.length; ++i) {
        var itm = document.getElementById(objs[i].name);
        if (itm == null) {
            continue;
        }
        switch (itm.type) {
            case "text":
                itm.value = objs[i].value;
                break;
            case "checkbox":
                itm.checked = objs[i].value;
                break;
            case "radio":
                var radios = document.getElementsByName(objs[i].name);
                var ind = parseInt(objs[i].value);
                radios[ind].checked = true;
                break;
        }
    }
});
